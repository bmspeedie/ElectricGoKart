#!/usr/bin/env python3
"""Read STM8 PC1/PC2 and UART2 through an Analog Discovery.

Wiring expected by this script:
  DIO 0 and DIO 8: PC1 and PC2 (either order)
  DIO 1 and DIO 9: UART TX and RX (either order)
"""

import sys
import time
from ctypes import (
    CDLL, byref, c_double, c_int, c_uint, create_string_buffer, sizeof,
)

sys.path.insert(0, "/usr/share/digilent/waveforms/samples/py")
from dwfconstants import DwfParamOnClose, hdwfNone  # noqa: E402

dwf = CDLL("libdwf.so")


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
    check(dwf.FDwfDeviceOpen(c_int(-1), byref(hdwf)), "open Analog Discovery")
    if hdwf.value == hdwfNone.value:
        raise RuntimeError(f"no Analog Discovery: {last_error()}")
    name = create_string_buffer(32)
    dwf.FDwfEnumDeviceName(c_int(0), name)
    version = create_string_buffer(32)
    dwf.FDwfGetVersion(version)
    print(f"device: {name.value.decode(errors='replace')}  dwf {version.value.decode()}")
    return hdwf


def read_inputs(hdwf):
    dwf.FDwfDigitalIOOutputEnableSet(hdwf, c_uint(0))
    dwf.FDwfDigitalIOPullSet(hdwf, c_uint(0), c_uint(0))
    check(dwf.FDwfDigitalIOConfigure(hdwf), "configure digital inputs")
    dwf.FDwfDigitalIOStatus(hdwf)
    value = c_uint()
    check(dwf.FDwfDigitalIOInputStatus(hdwf, byref(value)), "read digital inputs")
    return value.value


def sample_pins(hdwf, seconds, period=0.01):
    """Return (t, dio0, dio8, dio1, dio9) samples. Pins stay inputs."""
    rows = []
    end = time.perf_counter() + seconds
    while time.perf_counter() < end:
        t = time.perf_counter()
        value = read_inputs(hdwf)
        rows.append((
            t,
            (value >> 0) & 1,
            (value >> 8) & 1,
            (value >> 1) & 1,
            (value >> 9) & 1,
        ))
        time.sleep(period)
    t0 = rows[0][0]
    return [(t - t0, a, b, c, d) for t, a, b, c, d in rows]


def edges(rows, index):
    count = 0
    last = rows[0][index]
    for row in rows[1:]:
        if row[index] != last:
            count += 1
            last = row[index]
    return count


def high_fraction(rows, index):
    return sum(row[index] for row in rows) / len(rows)


def pulse_widths(rows, index):
    """High and low run lengths in seconds."""
    highs = []
    lows = []
    start = rows[0][0]
    level = rows[0][index]
    for t, *bits in ((row[0], row[index]) for row in rows[1:]):
        bit = bits[0]
        if bit != level:
            width = t - start
            (highs if level else lows).append(width)
            start = t
            level = bit
    return highs, lows


def describe_gpio(rows):
    print(f"samples: {len(rows)} over {rows[-1][0]:.2f}s")
    names = ("DIO0", "DIO8", "DIO1", "DIO9")
    for i, name in enumerate(names, start=1):
        print(
            f"  {name}: edges={edges(rows, i):3d}  "
            f"high={high_fraction(rows, i):.0%}  "
            f"last={rows[-1][i]}"
        )


def listen_uart(hdwf, rx_pin, seconds, baud=9600):
    """Listen only. TX is parked on an unused pin so the MCU TX wire is not driven."""
    unused_tx = 2
    check(dwf.FDwfDigitalUartRateSet(hdwf, c_double(baud)), "uart rate")
    check(dwf.FDwfDigitalUartBitsSet(hdwf, c_int(8)), "uart bits")
    check(dwf.FDwfDigitalUartParitySet(hdwf, c_int(0)), "uart parity")
    check(dwf.FDwfDigitalUartStopSet(hdwf, c_double(1)), "uart stop")
    check(dwf.FDwfDigitalUartTxSet(hdwf, c_int(unused_tx)), "uart tx pin")
    check(dwf.FDwfDigitalUartRxSet(hdwf, c_int(rx_pin)), "uart rx pin")
    got = c_int()
    parity = c_int()
    check(dwf.FDwfDigitalUartTx(hdwf, None, c_int(0)), "uart tx idle")
    check(dwf.FDwfDigitalUartRx(hdwf, None, c_int(0), byref(got), byref(parity)), "uart rx arm")

    buf = create_string_buffer(4096)
    data = bytearray()
    end = time.perf_counter() + seconds
    while time.perf_counter() < end:
        time.sleep(0.02)
        check(
            dwf.FDwfDigitalUartRx(hdwf, buf, c_int(sizeof(buf) - 1), byref(got), byref(parity)),
            "uart rx",
        )
        if got.value > 0:
            data += buf.raw[: got.value]
    return bytes(data), parity.value


def send_uart(hdwf, tx_pin, rx_pin, payload, seconds=1.5, baud=9600):
    check(dwf.FDwfDigitalUartTxSet(hdwf, c_int(tx_pin)), "uart tx pin")
    check(dwf.FDwfDigitalUartRxSet(hdwf, c_int(rx_pin)), "uart rx pin")
    got = c_int()
    parity = c_int()
    check(dwf.FDwfDigitalUartTx(hdwf, None, c_int(0)), "uart tx idle")
    check(dwf.FDwfDigitalUartRx(hdwf, None, c_int(0), byref(got), byref(parity)), "uart rx arm")
    time.sleep(0.05)
    raw = payload.encode()
    check(dwf.FDwfDigitalUartTx(hdwf, raw, c_int(len(raw))), "uart tx")
    buf = create_string_buffer(4096)
    data = bytearray()
    end = time.perf_counter() + seconds
    while time.perf_counter() < end:
        time.sleep(0.02)
        dwf.FDwfDigitalUartRx(hdwf, buf, c_int(sizeof(buf) - 1), byref(got), byref(parity))
        if got.value > 0:
            data += buf.raw[: got.value]
    return bytes(data)


def main():
    hdwf = open_device()
    try:
        print("sampling DIO 0, 8, 1, 9 for 3.2s")
        rows = sample_pins(hdwf, 3.2)
        describe_gpio(rows)

        # UART candidates are the pins that are not the slow GPIO pair.
        # Try both DIO1 and DIO9 as RX; the MCU TX line is the one that talks.
        for rx in (1, 9):
            print(f"listening for UART 9600 on DIO{rx}")
            data, _parity = listen_uart(hdwf, rx, 2.2)
            text = data.decode("ascii", errors="replace")
            print(f"  DIO{rx} got {len(data)} bytes: {text!r}")
            if b"PINTEST" in data or b"PC1" in data:
                other = 9 if rx == 1 else 1
                print(f"MCU TX is DIO{rx}. Sending echo probe on DIO{other}")
                echoed = send_uart(hdwf, other, rx, "Z")
                print(f"  echo reply: {echoed.decode('ascii', errors='replace')!r}")
                break
    finally:
        dwf.FDwfDeviceClose(hdwf)


if __name__ == "__main__":
    main()
