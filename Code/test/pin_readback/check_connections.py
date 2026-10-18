#!/usr/bin/env python3
"""Check the Analog Discovery wiring against the live controller.

  Analog 1: throttle
  DIO 8, 1, 9, 2, 10, 3: the six phase PWM wires, in that order
  DIO 0: UART
"""

import sys
import time
from ctypes import (
    CDLL, byref, c_byte, c_double, c_int, c_uint, c_uint16, create_string_buffer, sizeof,
)

sys.path.insert(0, "/usr/share/digilent/waveforms/samples/py")
from dwfconstants import (  # noqa: E402
    DwfParamOnClose,
    DwfStateDone,
    filterDecimate,
    hdwfNone,
)

dwf = CDLL("libdwf.so")

PWM_PINS = (8, 1, 9, 2, 10, 3)
UART_PIN = 0
CARRIERS_HZ = (15625.0, 20833.0)


def last_error():
    msg = create_string_buffer(512)
    dwf.FDwfGetLastErrorMsg(msg)
    return msg.value.decode(errors="replace")


def check(ok, what):
    if not ok:
        raise RuntimeError(f"{what}: {last_error()}")


def open_device():
    dwf.FDwfParamSet(DwfParamOnClose, c_int(1))
    hdwf = c_int()
    check(dwf.FDwfDeviceOpen(c_int(-1), byref(hdwf)), "open")
    if hdwf.value == hdwfNone.value:
        raise RuntimeError(last_error())
    name = create_string_buffer(32)
    version = create_string_buffer(32)
    dwf.FDwfEnumDeviceName(c_int(0), name)
    dwf.FDwfGetVersion(version)
    print(f"device: {name.value.decode()}  dwf {version.value.decode()}")
    return hdwf


def capture_analog(hdwf, n=4000, hz=200000.0):
    """Return (ch1_samples, ch2_samples) in volts. Channel 1 is scope input 1."""
    dwf.FDwfAnalogInFrequencySet(hdwf, c_double(hz))
    dwf.FDwfAnalogInBufferSizeSet(hdwf, c_int(n))
    dwf.FDwfAnalogInChannelEnableSet(hdwf, c_int(-1), c_int(1))
    dwf.FDwfAnalogInChannelOffsetSet(hdwf, c_int(-1), c_double(0))
    dwf.FDwfAnalogInChannelRangeSet(hdwf, c_int(-1), c_double(50))
    dwf.FDwfAnalogInChannelFilterSet(hdwf, c_int(-1), filterDecimate)
    check(dwf.FDwfAnalogInConfigure(hdwf, c_int(1), c_int(0)), "analog configure")
    time.sleep(2.0)
    check(dwf.FDwfAnalogInConfigure(hdwf, c_int(0), c_int(1)), "analog start")
    sts = c_byte()
    deadline = time.perf_counter() + 3
    while time.perf_counter() < deadline:
        dwf.FDwfAnalogInStatus(hdwf, c_int(1), byref(sts))
        if sts.value == DwfStateDone.value:
            break
        time.sleep(0.01)
    else:
        raise RuntimeError("analog capture timed out")
    channels = []
    for idx in (0, 1):
        buf = (c_double * n)()
        check(dwf.FDwfAnalogInStatusData(hdwf, c_int(idx), buf, c_int(n)), "analog data")
        channels.append([buf[i] for i in range(n)])
    return channels


def summarize_volts(samples):
    mean = sum(samples) / len(samples)
    var = sum((v - mean) ** 2 for v in samples) / len(samples)
    return min(samples), max(samples), mean, var ** 0.5


def capture_digital(hdwf, sample_hz=10_000_000):
    clock = c_double()
    check(dwf.FDwfDigitalInInternalClockInfo(hdwf, byref(clock)), "dio clock")
    divider = max(1, int(round(clock.value / sample_hz)))
    actual = clock.value / divider
    nmax = c_int()
    check(dwf.FDwfDigitalInBufferSizeInfo(hdwf, byref(nmax)), "dio buffer")
    n = min(8192, nmax.value)
    dwf.FDwfDigitalInDividerSet(hdwf, c_int(divider))
    dwf.FDwfDigitalInSampleFormatSet(hdwf, c_int(16))
    dwf.FDwfDigitalInBufferSizeSet(hdwf, c_int(n))
    dwf.FDwfDigitalInTriggerSourceSet(hdwf, c_byte(0))  # none
    check(dwf.FDwfDigitalInConfigure(hdwf, c_int(1), c_int(1)), "dio start")
    sts = c_byte()
    deadline = time.perf_counter() + 2
    while time.perf_counter() < deadline:
        dwf.FDwfDigitalInStatus(hdwf, c_int(1), byref(sts))
        if sts.value == DwfStateDone.value:
            break
        time.sleep(0.005)
    else:
        raise RuntimeError("digital capture timed out")
    raw = (c_uint16 * n)()
    check(dwf.FDwfDigitalInStatusData(hdwf, raw, c_int(2 * n)), "dio data")
    dwf.FDwfDigitalInReset(hdwf)
    return actual, [raw[i] for i in range(n)]


def bit_trace(raw, pin):
    mask = 1 << pin
    return [1 if word & mask else 0 for word in raw]


def analyze_trace(bits, sample_hz):
    rises = [i for i in range(1, len(bits)) if bits[i] == 1 and bits[i - 1] == 0]
    duty = sum(bits) / len(bits)
    if len(rises) < 2:
        level = "high" if bits[-1] else "low"
        return {"kind": "static", "level": level, "duty": duty, "edges": len(rises)}
    periods = [rises[i + 1] - rises[i] for i in range(len(rises) - 1)]
    avg = sum(periods) / len(periods)
    freq = sample_hz / avg
    nearest = min(CARRIERS_HZ, key=lambda f: abs(f - freq))
    kind = "pwm" if abs(freq - nearest) / nearest < 0.08 else "switching"
    return {
        "kind": kind,
        "freq": freq,
        "duty": duty,
        "edges": len(rises),
        "period_spread": (max(periods) - min(periods)) / avg,
    }


def driven_or_floating(hdwf, pins):
    """A driven pin ignores the weak pull. A floating pin follows it."""
    result = {}
    for pull_high in (False, True):
        mask = 0
        for pin in pins:
            mask |= 1 << pin
        up = mask if pull_high else 0
        down = 0 if pull_high else mask
        dwf.FDwfDigitalIOOutputEnableSet(hdwf, c_uint(0))
        dwf.FDwfDigitalIOPullSet(hdwf, c_uint(up), c_uint(down))
        check(dwf.FDwfDigitalIOConfigure(hdwf), "pull")
        time.sleep(0.02)
        highs = {pin: 0 for pin in pins}
        for _ in range(20):
            dwf.FDwfDigitalIOStatus(hdwf)
            value = c_uint()
            dwf.FDwfDigitalIOInputStatus(hdwf, byref(value))
            for pin in pins:
                highs[pin] += (value.value >> pin) & 1
            time.sleep(0.002)
        for pin in pins:
            result.setdefault(pin, {})["up" if pull_high else "down"] = highs[pin]
    dwf.FDwfDigitalIOPullSet(hdwf, c_uint(0), c_uint(0))
    dwf.FDwfDigitalIOConfigure(hdwf)
    verdict = {}
    for pin, counts in result.items():
        # 20 samples. Driven: both pulls agree. Floating: follows the pull.
        follows = counts["down"] <= 4 and counts["up"] >= 16
        driven_high = counts["down"] >= 16 and counts["up"] >= 16
        driven_low = counts["down"] <= 4 and counts["up"] <= 4
        if follows:
            verdict[pin] = "floating"
        elif driven_high:
            verdict[pin] = "driven high"
        elif driven_low:
            verdict[pin] = "driven low"
        else:
            verdict[pin] = f"active ({counts['down']}/20 low-pull high, {counts['up']}/20 high-pull high)"
    return verdict


def listen_uart(hdwf, rx_pin, seconds=2.0):
    got = c_int()
    parity = c_int()
    check(dwf.FDwfDigitalUartRateSet(hdwf, c_double(9600)), "uart rate")
    check(dwf.FDwfDigitalUartBitsSet(hdwf, c_int(8)), "uart bits")
    check(dwf.FDwfDigitalUartParitySet(hdwf, c_int(0)), "uart parity")
    check(dwf.FDwfDigitalUartStopSet(hdwf, c_double(1)), "uart stop")
    check(dwf.FDwfDigitalUartTxSet(hdwf, c_int(4)), "uart tx park")
    check(dwf.FDwfDigitalUartRxSet(hdwf, c_int(rx_pin)), "uart rx")
    check(dwf.FDwfDigitalUartTx(hdwf, None, c_int(0)), "uart idle")
    check(dwf.FDwfDigitalUartRx(hdwf, None, c_int(0), byref(got), byref(parity)), "uart arm")
    buf = create_string_buffer(4096)
    data = bytearray()
    end = time.perf_counter() + seconds
    while time.perf_counter() < end:
        time.sleep(0.02)
        dwf.FDwfDigitalUartRx(hdwf, buf, c_int(sizeof(buf) - 1), byref(got), byref(parity))
        if got.value > 0:
            data += buf.raw[: got.value]
    dwf.FDwfDigitalUartReset(hdwf)
    return bytes(data)


def main():
    hdwf = open_device()
    try:
        print("\nthrottle, scope inputs, 20 ms at 200 kHz, +/-25 V")
        ch1, ch2 = capture_analog(hdwf)
        for label, samples in (("analog 1", ch1), ("analog 2", ch2)):
            lo, hi, mean, sd = summarize_volts(samples)
            print(f"  {label}: {mean:+.3f} V   min {lo:+.3f}  max {hi:+.3f}  ripple {sd * 1000:.1f} mV")

        print("\nPWM pins, 10 MHz capture")
        sample_hz, raw = capture_digital(hdwf)
        print(f"  {len(raw)} samples at {sample_hz/1e6:.3f} MHz ({len(raw)/sample_hz*1e6:.0f} us)")
        reports = {}
        for pin in PWM_PINS:
            reports[pin] = analyze_trace(bit_trace(raw, pin), sample_hz)
            info = reports[pin]
            if info["kind"] == "static":
                print(f"  DIO {pin:2d}: static {info['level']}")
            else:
                print(
                    f"  DIO {pin:2d}: {info['kind']}  {info['freq']:.0f} Hz  "
                    f"duty {info['duty']:.0%}  edges {info['edges']}"
                )

        uart_trace = analyze_trace(bit_trace(raw, UART_PIN), sample_hz)
        if uart_trace["kind"] == "static":
            print(f"  DIO {UART_PIN} UART window: static {uart_trace['level']}")
        else:
            print(
                f"  DIO {UART_PIN} UART window: {uart_trace['kind']}  "
                f"{uart_trace['freq']:.0f} Hz  duty {uart_trace['duty']:.0%}"
            )

        static_pins = [pin for pin in PWM_PINS if reports[pin]["kind"] == "static"]
        if uart_trace["kind"] == "static":
            static_pins.append(UART_PIN)
        if static_pins:
            print("\npull test on pins that were not switching")
            verdict = driven_or_floating(hdwf, static_pins)
            for pin in static_pins:
                name = "UART" if pin == UART_PIN else "PWM"
                print(f"  DIO {pin:2d} {name}: {verdict[pin]}")

        print("\nUART 9600 on DIO 0 for 2 s")
        data = listen_uart(hdwf, UART_PIN)
        text = data.decode("ascii", errors="replace")
        print(f"  {len(data)} bytes: {text!r}")
    finally:
        dwf.FDwfDeviceClose(hdwf)


if __name__ == "__main__":
    main()
