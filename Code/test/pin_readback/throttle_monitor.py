#!/usr/bin/env python3
"""Live throttle monitor.

Analog Discovery scope channel 1 is the throttle voltage.
UART on DIO 0 carries the controller report: thr <adc> duty <n> hall <n>.
Position is that ADC reading along the firmware travel, 0% at the rest
deadzone (ADC 53) and 100% at ADC 182.

Open http://127.0.0.1:8765
"""

import json
import re
import sys
import threading
import time
from ctypes import CDLL, byref, c_byte, c_double, c_int, c_uint, c_uint16, create_string_buffer
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

sys.path.insert(0, "/usr/share/digilent/waveforms/samples/py")
from dwfconstants import (  # noqa: E402
    DwfParamOnClose,
    DwfStateDone,
    hdwfNone,
    trigsrcDetectorDigitalIn,
)

dwf = CDLL("libdwf.so")

HOST = "127.0.0.1"
PORT = 8765
ADC_POSITION_MIN = 53  # ADC_THROTTLE_MIN_VALUE 43 + GOKART_THROTTLE_DEADZONE 10
ADC_POSITION_MAX = 182
UART_RX = 0
HALL_PINS = [6, 7, 15]  # DIO lines wired to the three hall sensor outputs
BAUD = 9600.0
UART_SAMPLE_HZ = 100_000.0  # about 10.4 samples per bit
UART_SAMPLES = 16384  # 164 ms window: covers a full report period (~120 ms) plus LCD3 frames

# Scope volts to controller ADC counts, fitted from matched readings
# (0.85 V -> 45, 3.65 V -> 174). Used when the controller sends no report.
ADC_PER_VOLT = 46.1
ADC_OFFSET = 5.8

state = {
    "voltage": None,
    "vplus": None,
    "voltage_max": None,
    "adc": None,
    "adc_max": None,
    "dr": None,
    "position": None,
    "position_max": None,
    "duty": None,
    "hall": None,
    "uart_at": 0.0,
    "frame_at": 0.0,
    "frame": None,
    "bytes_at": 0.0,
    "hall_levels": None,
    "hall_edges": [0, 0, 0],
    "hall_seq": None,
    "hall_seq_total": {"captures": 0, "transitions": 0, "fwd": 0, "back": 0, "invalid": 0, "multibit": 0, "glitches": 0},
    # MCU-side hall counters from the telemetry line, accumulated since reset
    "mcu": {"last_hc": None, "last_hi": None, "changes": 0, "invalid": 0, "erps": None, "t0": None, "t_last": None},
}
lock = threading.Lock()
history = []  # (time, scope V, adc, duty, erps from MCU, erps from scope, hall, ang, cur) per controller report
ib_log = []  # (time, duty, erps, [32 phase-B current samples by applied angle])


def last_error():
    msg = create_string_buffer(512)
    dwf.FDwfGetLastErrorMsg(msg)
    return msg.value.decode(errors="replace")


def check(ok, what):
    if not ok:
        raise RuntimeError(f"{what}: {last_error()}")


def position_percent(adc):
    span = ADC_POSITION_MAX - ADC_POSITION_MIN
    return max(0.0, min(100.0, (adc - ADC_POSITION_MIN) * 100.0 / span))


def enable_throttle_supply(hdwf):
    """Turn on V+ at THROTTLE_SUPPLY_V if it is not already on."""
    en = c_double()
    dwf.FDwfAnalogIOChannelNodeGet(hdwf, c_int(0), c_int(0), byref(en))
    if en.value:
        return
    dwf.FDwfAnalogIOChannelNodeSet(hdwf, c_int(0), c_int(0), c_double(1))
    dwf.FDwfAnalogIOChannelNodeSet(hdwf, c_int(0), c_int(1), c_double(THROTTLE_SUPPLY_V))
    dwf.FDwfAnalogIOEnableSet(hdwf, c_int(1))
    print(f"V+ supply enabled at {THROTTLE_SUPPLY_V:.1f} V for the throttle", flush=True)


def pick_config():
    """Choose the device configuration with the deepest logic-analyser buffer
    that still has a scope channel (Analog Discovery 2: config 7 = 16k)."""
    DECIAnalogInChannelCount, DECIDigitalInBufferSize = 1, 9
    count = c_int()
    dwf.FDwfEnum(c_int(0), byref(count))
    if count.value < 1:
        raise RuntimeError("no Analog Discovery found")
    cfgs = c_int()
    dwf.FDwfEnumConfig(c_int(0), byref(cfgs))
    best, best_buf = 0, -1
    for i in range(cfgs.value):
        ai, di = c_int(), c_int()
        dwf.FDwfEnumConfigInfo(c_int(i), c_int(DECIAnalogInChannelCount), byref(ai))
        dwf.FDwfEnumConfigInfo(c_int(i), c_int(DECIDigitalInBufferSize), byref(di))
        if ai.value >= 1 and di.value > best_buf:
            best, best_buf = i, di.value
    return best


THROTTLE_SUPPLY_V = 5.0  # throttle is powered from the Discovery V+ rail


def open_device():
    # 0 = leave instruments (including the V+ supply) running when we close,
    # so restarting scripts does not cut the throttle's 5 V.
    dwf.FDwfParamSet(DwfParamOnClose, c_int(0))
    hdwf = c_int()
    cfg = pick_config()
    check(dwf.FDwfDeviceConfigOpen(c_int(0), c_int(cfg), byref(hdwf)), "open Analog Discovery")
    enable_throttle_supply(hdwf)
    if hdwf.value == hdwfNone.value:
        raise RuntimeError(last_error())
    return hdwf


def setup(hdwf):
    dwf.FDwfAnalogInChannelEnableSet(hdwf, c_int(0), c_int(1))
    dwf.FDwfAnalogInChannelOffsetSet(hdwf, c_int(0), c_double(0))
    dwf.FDwfAnalogInChannelRangeSet(hdwf, c_int(0), c_double(25))
    check(dwf.FDwfAnalogInConfigure(hdwf, c_int(1), c_int(0)), "analog configure")

    # Logic capture on the UART pin. Trigger only on a low that lasts at
    # least half a bit, so sub-microsecond noise spikes do not start a capture.
    clock = c_double()
    check(dwf.FDwfDigitalInInternalClockInfo(hdwf, byref(clock)), "digital clock")
    divider = int(round(clock.value / UART_SAMPLE_HZ))
    dwf.FDwfDigitalInDividerSet(hdwf, c_uint(divider))
    dwf.FDwfDigitalInSampleFormatSet(hdwf, c_int(16))
    max_buf = c_int()
    dwf.FDwfDigitalInBufferSizeInfo(hdwf, byref(max_buf))
    if max_buf.value and max_buf.value < UART_SAMPLES:
        raise RuntimeError(f"digital buffer only holds {max_buf.value} samples, need {UART_SAMPLES}")
    dwf.FDwfDigitalInBufferSizeSet(hdwf, c_int(UART_SAMPLES))
    dwf.FDwfDigitalInTriggerSourceSet(hdwf, trigsrcDetectorDigitalIn)
    dwf.FDwfDigitalInTriggerPositionSet(hdwf, c_uint(UART_SAMPLES - 8))
    dwf.FDwfDigitalInTriggerSet(hdwf, c_uint(0), c_uint(0), c_uint(0), c_uint(1 << UART_RX))
    check(
        dwf.FDwfDigitalInTriggerLengthSet(hdwf, c_double(0.5 / BAUD), c_double(-1), c_int(0)),
        "uart trigger length",
    )
    time.sleep(1.5)


def majority(bits, start, count):
    ones = sum(bits[start:start + count])
    return 1 if ones * 2 > count else 0


def decode_uart(bits, samples_per_bit):
    """8N1 decode with per-bit majority vote. Returns the decoded bytes."""
    out = bytearray()
    i = 0
    n = len(bits)
    spb = samples_per_bit
    half = int(spb / 2)
    vote = max(3, int(spb * 0.6))
    while i < n - int(spb * 10):
        if bits[i] != 0:
            i += 1
            continue
        # candidate start bit: the middle of it has to read low
        centre = i + half
        if majority(bits, centre - vote // 2, vote) != 0:
            i += 1
            continue
        value = 0
        for b in range(8):
            c = int(i + spb * (1.5 + b))
            value |= majority(bits, c - vote // 2, vote) << b
        stop_c = int(i + spb * 9.5)
        if majority(bits, stop_c - vote // 2, vote) == 1:
            out.append(value)
            i = int(i + spb * 9.5)
        else:
            i += 1
    return bytes(out)


def capture_uart(hdwf, timeout=0.3):
    """Arm one capture and return decoded bytes, or b'' if nothing arrived."""
    check(dwf.FDwfDigitalInConfigure(hdwf, c_int(1), c_int(1)), "uart capture arm")
    sts = c_byte()
    deadline = time.perf_counter() + timeout
    while time.perf_counter() < deadline:
        dwf.FDwfDigitalInStatus(hdwf, c_int(1), byref(sts))
        if sts.value == DwfStateDone.value:
            break
        time.sleep(0.005)
    else:
        dwf.FDwfDigitalInConfigure(hdwf, c_int(0), c_int(0))
        return b""
    raw = (c_uint16 * UART_SAMPLES)()
    check(dwf.FDwfDigitalInStatusData(hdwf, raw, c_int(2 * UART_SAMPLES)), "uart capture data")
    words = list(raw)
    bits = [(w >> UART_RX) & 1 for w in words]
    analyse_hall_sequence(words)
    return decode_uart(bits, UART_SAMPLE_HZ / BAUD)


HALL_CYCLE = [1, 3, 2, 6, 4, 5]  # valid 120-degree hall order, one rotation sense


def analyse_hall_sequence(words):
    """Run-length encode the three hall lines from a logic capture and judge
    the commutation sequence: direction, invalid states, multi-bit jumps,
    sub-100 us glitches and per-state dwell."""
    a, b, c = HALL_PINS
    seq = [((w >> a) & 1) | (((w >> b) & 1) << 1) | (((w >> c) & 1) << 2) for w in words]
    runs = []
    cur, start = seq[0], 0
    for i in range(1, len(seq)):
        if seq[i] != cur:
            runs.append((cur, i - start))
            cur, start = seq[i], i
    runs.append((cur, len(seq) - start))

    glitch_len = int(UART_SAMPLE_HZ * 100e-6)  # samples in 100 us
    kept = [r for r in runs if r[1] > glitch_len]
    glitches = len(runs) - len(kept)
    clean = []  # merge the pieces a glitch split apart
    for s, n in kept:
        if clean and clean[-1][0] == s:
            clean[-1] = (s, clean[-1][1] + n)
        else:
            clean.append((s, n))
    states = [r[0] for r in clean]
    invalid = sum(1 for s in states if s in (0, 7))
    fwd = back = multibit = 0
    for x, y in zip(states, states[1:]):
        if bin(x ^ y).count("1") != 1:
            multibit += 1
        if x in HALL_CYCLE and y in HALL_CYCLE:
            d = (HALL_CYCLE.index(y) - HALL_CYCLE.index(x)) % 6
            if d == 1:
                fwd += 1
            elif d == 5:
                back += 1
    dwell = {}
    for s, n in clean[1:-1]:
        dwell.setdefault(s, []).append(n * 1000.0 / UART_SAMPLE_HZ)
    window = len(seq) / UART_SAMPLE_HZ
    transitions = max(0, len(clean) - 1)
    with lock:
        state["hall_seq"] = {
            "states": states[-48:],
            "transitions": transitions,
            "erps": transitions / 6.0 / window,
            "fwd": fwd,
            "back": back,
            "invalid": invalid,
            "multibit": multibit,
            "glitches": glitches,
            "dwell_ms": {str(s): round(sum(v) / len(v), 2) for s, v in sorted(dwell.items())},
            "window_ms": round(window * 1000),
        }
        tot = state["hall_seq_total"]
        tot["window_s"] = tot.get("window_s", 0.0) + window
        tot["all_transitions"] = tot.get("all_transitions", 0) + transitions
        tot["all_invalid"] = tot.get("all_invalid", 0) + invalid
        if transitions >= 6:
            tot["transitions"] += transitions
            tot["fwd"] += fwd
            tot["back"] += back
            tot["invalid"] += invalid
            tot["multibit"] += multibit
            tot["glitches"] += glitches
            tot["captures"] += 1


def read_voltage(hdwf):
    voltage = c_double()
    sts = c_byte()
    dwf.FDwfAnalogInStatus(hdwf, c_int(0), byref(sts))
    check(dwf.FDwfAnalogInStatusSample(hdwf, c_int(0), byref(voltage)), "analog sample")
    # V+ rail readback (actual, not setpoint)
    vplus = c_double()
    dwf.FDwfAnalogIOStatus(hdwf)
    dwf.FDwfAnalogIOChannelNodeStatus(hdwf, c_int(0), c_int(1), byref(vplus))
    with lock:
        state["vplus"] = vplus.value
    return voltage.value


def lcd3_frames(data):
    """Yield valid 12-byte KT-LCD3 frames: 0x41 header, B6 = xor of B1..B11 without B6."""
    for i in range(len(data) - 11):
        if data[i] != 0x41:
            continue
        frame = data[i:i + 12]
        crc = 0
        for j in range(1, 12):
            if j != 6:
                crc ^= frame[j]
        if crc == frame[6]:
            yield frame


REPORT_RE = re.compile(
    rb"thr (\d{1,3}) dr (\d{1,3}) duty (\d{1,3}) hall (-?\d{1,2})"
    rb"(?: hc (\d{1,5}) hi (\d{1,5}) erps (\d{1,5}))?(?: ang (\d{1,3}))?(?: cur (-?\d{1,4}))?(?: tmin (\d{1,3}) tmax (\d{1,3}))?(?: rst (\d{1,3}) up (\d{1,3}))?\r?\n"
)


def text_reports(data):
    """Yield parsed 'thr N dr N duty N hall N [hc N hi N erps N]' lines that
    are complete within this capture. The line may be glued directly onto
    LCD3 frame bytes, so match the pattern anywhere rather than splitting."""
    for m in REPORT_RE.finditer(data):
        thr, dr, duty, hall = (int(g) for g in m.groups()[:4])
        if thr <= 255 and dr <= 255 and duty <= 255:
            r = {"thr": thr, "dr": dr, "duty": duty, "hall": hall}
            if m.group(5) is not None:
                r["hc"] = int(m.group(5))
                r["hi"] = int(m.group(6))
                r["erps"] = int(m.group(7))
            if m.group(8) is not None:
                r["ang"] = int(m.group(8))
            if m.group(9) is not None:
                r["cur"] = int(m.group(9))
            if m.group(10) is not None:
                r["tmin"], r["tmax"] = int(m.group(10)), int(m.group(11))
            if m.group(12) is not None:
                r["rst"], r["up"] = int(m.group(12)), int(m.group(13))
            yield r


SHORT_RE = re.compile(rb"thr (\d{1,3}) up (\d{1,3}) rst (\d{1,3})\r?\n")
IB_RE = re.compile(rb"ib((?: \d{1,3}){32})\r?\n")


def ib_waves(data):
    """Yield phase-B current-vs-angle waves (32 ints) from 'ib v0 .. v31' lines."""
    for m in IB_RE.finditer(data):
        yield [int(v) for v in m.group(1).split()]


def _track_position_max(pos):
    state["position_max"] = pos if state["position_max"] is None else max(state["position_max"], pos)


def read_uart(hdwf):
    data = capture_uart(hdwf)
    if not data:
        return
    now = time.time()
    with lock:
        state["bytes_at"] = now
    for frame in lcd3_frames(data):
        with lock:
            state["frame_at"] = now
            state["frame"] = frame.hex(" ")
    for m in SHORT_RE.finditer(data):
        with lock:
            state["uart_at"] = now
            history.append((round(now, 2), round(state["voltage"] or 0, 3), int(m.group(1)), 0, 0, 0.0, state["hall"],
                            None, None, None, None, int(m.group(3)), int(m.group(2))))
    for wave in ib_waves(data):
        with lock:
            state["ib_wave"] = wave
            ib_log.append((round(now, 2), state["duty"], state["mcu"]["erps"], wave))
            if len(ib_log) > 200:
                del ib_log[:len(ib_log) - 200]
    for parsed in text_reports(data):
        adc = parsed["thr"]
        pos = position_percent(adc)
        with lock:
            state["adc"] = adc
            state["adc_max"] = adc if state["adc_max"] is None else max(state["adc_max"], adc)
            state["dr"] = parsed.get("dr", state["dr"])
            state["position"] = pos
            _track_position_max(pos)
            state["duty"] = parsed.get("duty", state["duty"])
            state["hall"] = parsed.get("hall", state["hall"])
            state["ang"] = parsed.get("ang", state.get("ang"))
            state["uart_at"] = now
            if "hc" in parsed:
                m = state["mcu"]
                if m["last_hc"] is not None:
                    m["changes"] += (parsed["hc"] - m["last_hc"]) & 0xFFFF
                    m["invalid"] += (parsed["hi"] - m["last_hi"]) & 0xFFFF
                else:
                    m["t0"] = now
                m["last_hc"], m["last_hi"] = parsed["hc"], parsed["hi"]
                m["erps"] = parsed["erps"]
                m["t_last"] = now
            scope_erps = state["hall_seq"]["erps"] if state["hall_seq"] else None
            history.append((round(now, 2), round(state["voltage"] or 0, 3), adc, parsed.get("duty"),
                            parsed.get("erps"), None if scope_erps is None else round(scope_erps, 1), parsed.get("hall"),
                            parsed.get("ang"), parsed.get("cur"), parsed.get("tmin"), parsed.get("tmax"), parsed.get("rst"), parsed.get("up")))
            if len(history) > 2000:
                del history[:len(history) - 2000]


def read_halls(hdwf):
    """Sample the hall DIO lines; returns levels and updates edge counts."""
    dwf.FDwfDigitalIOStatus(hdwf)
    value = c_uint()
    dwf.FDwfDigitalIOInputStatus(hdwf, byref(value))
    levels = [(value.value >> p) & 1 for p in HALL_PINS]
    with lock:
        prev = state["hall_levels"]
        if prev is not None:
            for i, lv in enumerate(levels):
                if lv != prev[i]:
                    state["hall_edges"][i] += 1
        state["hall_levels"] = levels


def sample_loop(hdwf):
    while True:
        try:
            voltage = read_voltage(hdwf)
            with lock:
                state["voltage"] = voltage
                if state["voltage_max"] is None or voltage > state["voltage_max"]:
                    state["voltage_max"] = voltage
                if not state["uart_at"] or time.time() - state["uart_at"] > 1.5:
                    _track_position_max(position_percent(ADC_OFFSET + ADC_PER_VOLT * voltage))
            read_halls(hdwf)
            read_uart(hdwf)
            read_halls(hdwf)
        except Exception as exc:
            print(f"sample error: {exc}", flush=True)
            time.sleep(0.2)


PAGE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Throttle</title>
<style>
  body { margin: 0; font-family: sans-serif; background: #101010; color: #f2f2f2; }
  main { max-width: 640px; margin: 48px auto; padding: 0 24px; }
  h1 { font-size: 14px; font-weight: 600; letter-spacing: 0.04em; text-transform: uppercase; color: #9a9a9a; }
  .voltage { font-size: 64px; font-weight: 600; margin: 8px 0 28px; }
  .voltage span { font-size: 28px; color: #9a9a9a; margin-left: 8px; }
  .label { font-size: 14px; color: #9a9a9a; }
  .position { font-size: 40px; font-weight: 600; margin: 4px 0 12px; }
  .track { height: 18px; background: #2a2a2a; border-radius: 9px; overflow: hidden; }
  .fill { height: 100%; width: 0%; background: #3d9a6a; }
  .meta { margin-top: 22px; color: #bdbdbd; font-size: 15px; }
  .stale { color: #d0a24a; }
  .row { display: flex; gap: 32px; margin: 0 0 28px; }
  .cell { flex: 1; }
  .big { font-size: 48px; font-weight: 600; margin: 4px 0 2px; }
  .big span { font-size: 22px; color: #9a9a9a; margin-left: 6px; }
  .sub { font-size: 14px; color: #9a9a9a; }
  .seq { font-family: monospace; font-size: 22px; letter-spacing: 0.12em; margin: 6px 0 4px; word-break: break-all; }
  .bad { color: #d85a5a; }
</style>
</head>
<body>
<main>
  <div class="row">
    <div class="cell">
      <h1>Scope voltage</h1>
      <div class="big" id="voltage">– <span>V</span></div>
      <div class="sub" id="voltage_sub"></div>
    </div>
    <div class="cell">
      <h1>Controller ADC</h1>
      <div class="big" id="adc">–</div>
      <div class="sub" id="adc_sub"></div>
    </div>
  </div>
  <div class="label">Throttle position</div>
  <div class="position" id="position">–</div>
  <div class="track"><div class="fill" id="fill"></div></div>
  <div class="label" style="margin-top:28px">Hall sensors</div>
  <div class="big" id="halls">–</div>
  <div class="sub" id="halls_sub"></div>
  <div class="label" style="margin-top:20px">Commutation sequence (last capture)</div>
  <div class="seq" id="seq">–</div>
  <div class="sub" id="seq_sub"></div>
  <div class="sub" id="seq_total"></div>
  <div class="label" style="margin-top:20px">MCU vs scope hall changes</div>
  <div class="sub" id="mcu"></div>
  <div class="meta" id="meta"></div>
</main>
<script>
async function tick() {
  try {
    const r = await fetch('/state');
    const s = await r.json();
    document.getElementById('voltage').innerHTML =
      (s.voltage == null ? '–' : s.voltage.toFixed(2)) + ' <span>V</span>';
    document.getElementById('voltage_sub').textContent =
      s.voltage_adc_est == null ? '' : 'expect ADC ' + Math.round(s.voltage_adc_est);
    const adcEl = document.getElementById('adc');
    const adcSub = document.getElementById('adc_sub');
    if (s.adc == null) {
      adcEl.textContent = '–';
      adcEl.className = 'big stale';
      adcSub.textContent = s.adc_age == null ? 'no report yet' : 'last report ' + s.adc_age.toFixed(1) + ' s ago';
    } else {
      adcEl.innerHTML = s.adc + ' <span>/ 255</span>';
      adcEl.className = 'big';
      const bits = [];
      if (s.adc_volts_est != null) bits.push('≈ ' + s.adc_volts_est.toFixed(2) + ' V');
      if (s.adc_delta != null) bits.push('Δ ' + (s.adc_delta > 0 ? '+' : '') + s.adc_delta + ' vs scope');
      adcSub.textContent = bits.join(' · ');
    }
    const pos = s.position == null ? null : Math.round(s.position);
    document.getElementById('position').textContent = pos == null ? '–' : pos + '%';
    document.getElementById('fill').style.width = (pos == null ? 0 : pos) + '%';
    const hallsEl = document.getElementById('halls');
    const hallsSub = document.getElementById('halls_sub');
    if (s.hall_levels) {
      hallsEl.textContent = s.hall_levels.join(' ');
      const bad = s.hall_levels.every(v => v === 1) || s.hall_levels.every(v => v === 0);
      hallsEl.className = bad ? 'big stale' : 'big';
      const bits = ['DIO ' + s.hall_pins.join('/')];
      bits.push('edges ' + s.hall_edges.join('/'));
      if (s.hall != null) bits.push('controller hall ' + s.hall + ' (A' + (s.hall & 1) + ' B' + ((s.hall >> 1) & 1) + ' C' + ((s.hall >> 2) & 1) + ')');
      if (bad) bits.push(s.hall_levels[0] ? 'all high: sensors unpowered or disconnected' : 'all low: invalid state');
      hallsSub.textContent = bits.join(' · ');
    }
    const q = s.hall_seq;
    if (q) {
      const seqEl = document.getElementById('seq');
      if (q.transitions < 2) {
        seqEl.textContent = 'motor stopped (state ' + (q.states.length ? q.states[q.states.length - 1] : '–') + ')';
        seqEl.className = 'seq';
        document.getElementById('seq_sub').textContent = q.glitches ? q.glitches + ' glitches <100 µs filtered' : '';
      } else {
        seqEl.innerHTML = q.states.map(v => (v === 0 || v === 7) ? '<span class="bad">' + v + '</span>' : v).join('');
        seqEl.className = 'seq';
        const dir = q.fwd > q.back ? 'forward' : (q.back > q.fwd ? 'reverse' : 'undetermined');
        const bits = [q.erps.toFixed(0) + ' erps', dir + ' (' + q.fwd + '/' + q.back + ')'];
        bits.push((q.invalid ? 'INVALID states ' + q.invalid : 'no 0/7 states'));
        bits.push((q.multibit ? 'multi-bit jumps ' + q.multibit : 'single-bit steps'));
        if (q.glitches) bits.push(q.glitches + ' glitches filtered');
        const dw = Object.entries(q.dwell_ms).map(([k, v]) => k + ':' + v.toFixed(1)).join(' ');
        if (dw) bits.push('dwell ms ' + dw);
        document.getElementById('seq_sub').textContent = bits.join(' · ');
      }
      const t = s.hall_seq_total;
      document.getElementById('seq_total').textContent = t.captures
        ? 'running total: ' + t.transitions + ' steps, fwd ' + t.fwd + ', back ' + t.back + ', invalid ' + t.invalid + ', multi-bit ' + t.multibit + ', glitches ' + t.glitches + ' over ' + t.captures + ' captures'
        : '';
    }
    const m = s.mcu;
    const mcuEl = document.getElementById('mcu');
    if (m) {
      const bits = ['MCU saw ' + m.changes + ' hall changes (' + m.rate + '/s)'];
      bits.push('invalid 0/7: ' + m.invalid);
      if (m.scope_rate != null) bits.push('scope sees ' + m.scope_rate + '/s, invalid ' + m.scope_invalid);
      if (m.expected_rate != null) bits.push('6×erps = ' + m.expected_rate + '/s');
      bits.push('over ' + m.elapsed_s + ' s');
      mcuEl.textContent = bits.join(' · ');
      mcuEl.className = (m.invalid > 0 || (m.scope_rate != null && m.rate > m.scope_rate * 1.3 + 2)) ? 'sub bad' : 'sub';
    } else {
      mcuEl.textContent = 'no MCU hall counters in telemetry yet';
      mcuEl.className = 'sub stale';
    }
    const parts = [];
    if (s.position_source) parts.push('position from ' + s.position_source);
    if (s.dr != null) parts.push('ADC_DR ' + s.dr);
    if (s.duty != null) parts.push('duty ' + s.duty);
    if (s.hall != null) parts.push('hall ' + s.hall);
    if (s.voltage_max != null) parts.push('peak ' + s.voltage_max.toFixed(2) + ' V');
    if (s.vplus != null) parts.push('V+ ' + s.vplus.toFixed(2) + ' V');
    if (s.link) parts.push(s.link);
    const meta = document.getElementById('meta');
    meta.textContent = parts.join(' · ');
    meta.className = (s.position_source === 'controller') ? 'meta' : 'meta stale';
  } catch (e) {
    document.getElementById('meta').textContent = 'monitor disconnected';
  }
}
setInterval(tick, 200);
tick();
</script>
</body>
</html>
"""


def mcu_summary(snap):
    """Compare hall changes counted by the MCU with those the scope sees."""
    m = snap["mcu"]
    if m["t0"] is None or m["t_last"] is None or m["t_last"] <= m["t0"]:
        return None
    elapsed = m["t_last"] - m["t0"]
    tot = snap["hall_seq_total"]
    scope_rate = None
    if tot.get("window_s"):
        scope_rate = tot.get("all_transitions", 0) / tot["window_s"]
    return {
        "elapsed_s": round(elapsed, 1),
        "changes": m["changes"],
        "invalid": m["invalid"],
        "rate": round(m["changes"] / elapsed, 1),          # changes per second seen by MCU
        "scope_rate": None if scope_rate is None else round(scope_rate, 1),
        "scope_invalid": tot.get("all_invalid", 0),
        "erps": m["erps"],
        "expected_rate": None if m["erps"] is None else 6 * m["erps"],
    }


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/state"):
            with lock:
                snap = dict(state)
            now = time.time()
            report_age = None if not snap["uart_at"] else now - snap["uart_at"]
            frame_age = None if not snap["frame_at"] else now - snap["frame_at"]
            bytes_age = None if not snap["bytes_at"] else now - snap["bytes_at"]

            if report_age is not None and report_age < 1.5:
                position = snap["position"]
                source = "controller"
            elif snap["voltage"] is not None:
                position = position_percent(ADC_OFFSET + ADC_PER_VOLT * snap["voltage"])
                source = "scope"
            else:
                position = None
                source = None

            if report_age is not None and report_age < 1.5:
                link = "controller report"
            elif frame_age is not None and frame_age < 1.5:
                link = "controller sending LCD3 display frames, no throttle report"
            elif bytes_age is not None and bytes_age < 1.5:
                link = "controller sending unrecognised data"
            else:
                link = "controller silent"

            fresh = source == "controller"
            voltage_adc_est = None
            if snap["voltage"] is not None:
                voltage_adc_est = ADC_OFFSET + ADC_PER_VOLT * snap["voltage"]
            adc_volts_est = None
            adc_delta = None
            if fresh and snap["adc"] is not None:
                adc_volts_est = (snap["adc"] - ADC_OFFSET) / ADC_PER_VOLT
                if voltage_adc_est is not None:
                    adc_delta = int(round(snap["adc"] - voltage_adc_est))

            body = {
                "voltage": snap["voltage"],
                "vplus": snap["vplus"],
                "voltage_max": snap["voltage_max"],
                "voltage_adc_est": voltage_adc_est,
                "adc_volts_est": adc_volts_est,
                "adc_delta": adc_delta,
                "adc_age": report_age,
                "adc": snap["adc"] if source == "controller" else None,
                "dr": snap["dr"] if source == "controller" else None,
                "duty": snap["duty"] if source == "controller" else None,
                "hall": snap["hall"] if source == "controller" else None,
                "position": position,
                "position_source": source,
                "position_max": snap["position_max"],
                "link": link,
                "hall_pins": HALL_PINS,
                "hall_levels": snap["hall_levels"],
                "hall_edges": snap["hall_edges"],
                "hall_seq": snap["hall_seq"],
                "hall_seq_total": dict(snap["hall_seq_total"]),
                "mcu": mcu_summary(snap),
                "frame": snap["frame"],
                "uart_age": report_age,
            }
            raw = json.dumps(body).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)
            return
        if self.path.startswith("/history") or self.path.startswith("/ib"):
            with lock:
                raw = json.dumps(ib_log if self.path.startswith("/ib") else history).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)
            return
        if self.path.startswith("/reset"):
            with lock:
                state["hall_seq_total"] = {k: 0 for k in state["hall_seq_total"]}
                state["hall_edges"] = [0, 0, 0]
                m = state["mcu"]
                m.update(changes=0, invalid=0, t0=m["t_last"])
                del history[:]
                state["voltage_max"] = None
                state["position_max"] = None
                state["adc_max"] = None
            self.send_response(204)
            self.end_headers()
            return
        raw = PAGE.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def log_message(self, fmt, *args):
        return


def main():
    hdwf = open_device()
    setup(hdwf)
    threading.Thread(target=sample_loop, args=(hdwf,), daemon=True).start()
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"Throttle monitor at http://{HOST}:{PORT}", flush=True)
    try:
        server.serve_forever()
    finally:
        dwf.FDwfDeviceClose(hdwf)


if __name__ == "__main__":
    main()
