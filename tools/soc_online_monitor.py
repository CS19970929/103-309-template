#!/usr/bin/env python3
"""Read live BMS SOC status over Modbus RTU and optionally write CSV."""

from __future__ import annotations

import argparse
import csv
import sys
import time
from pathlib import Path


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def append_crc(frame: bytes) -> bytes:
    crc = crc16_modbus(frame)
    return frame + bytes((crc & 0xFF, (crc >> 8) & 0xFF))


def u16be(value: int) -> bytes:
    return bytes(((value >> 8) & 0xFF, value & 0xFF))


def build_read(slave: int, addr: int, count: int) -> bytes:
    return append_crc(bytes((slave, 0x03)) + u16be(addr) + u16be(count))


def verify_crc(frame: bytes) -> None:
    expected = crc16_modbus(frame[:-2])
    actual = frame[-2] | (frame[-1] << 8)
    if expected != actual:
        raise RuntimeError("CRC mismatch expected=0x{0:04X} actual=0x{1:04X}".format(expected, actual))


def read_status(ser, slave: int) -> dict[str, int]:
    request = build_read(slave, 0xD000, 63)
    ser.reset_input_buffer()
    ser.write(request)
    ser.flush()
    head = ser.read(3)
    if len(head) != 3:
        raise RuntimeError("timeout waiting response header")
    if head[1] & 0x80:
        tail = ser.read(2)
        raise RuntimeError("modbus exception code=0x{0:02X}".format(head[2] if len(tail) == 2 else 0))
    byte_count = head[2]
    body = ser.read(byte_count + 2)
    if len(body) != byte_count + 2:
        raise RuntimeError("timeout waiting response body")
    frame = head + body
    verify_crc(frame)
    payload = frame[3 : 3 + byte_count]
    regs = [(payload[i] << 8) | payload[i + 1] for i in range(0, len(payload), 2)]
    return {
        "vmax_mv": regs[32],
        "vmin_mv": regs[33],
        "vdelta_mv": regs[36],
        "total_v_x100": regs[37],
        "ichg_a10": regs[50],
        "idsg_a10": regs[51],
        "soc": regs[52],
        "soh": regs[53],
        "cap_now_ah100": regs[54],
        "cap_full_ah100": regs[55],
        "cap_factory_ah100": regs[56],
        "cycle_times": regs[57],
        "fault1": regs[58],
        "fault2": regs[59],
        "fault3": regs[60],
        "balance1": regs[61],
        "balance2": regs[62],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Monitor BMS SOC via Modbus RTU.")
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=19200)
    parser.add_argument("--slave", type=int, default=1)
    parser.add_argument("--interval", type=float, default=1.0)
    parser.add_argument("--samples", type=int, default=30)
    parser.add_argument("--csv")
    args = parser.parse_args()

    try:
        import serial  # type: ignore
    except ImportError:
        print("missing pyserial: py -m pip install pyserial", file=sys.stderr)
        return 2

    writer = None
    csv_file = None
    if args.csv:
        csv_file = Path(args.csv).open("w", newline="", encoding="utf-8")

    try:
        with serial.Serial(args.port, args.baud, timeout=max(0.2, args.interval)) as ser:
            for index in range(args.samples):
                row = read_status(ser, args.slave)
                row["sample"] = index
                row["time_s"] = round(index * args.interval, 3)
                if writer is None and csv_file is not None:
                    writer = csv.DictWriter(csv_file, fieldnames=list(row.keys()))
                    writer.writeheader()
                if writer is not None:
                    writer.writerow(row)
                    csv_file.flush()
                print(
                    "sample={sample} soc={soc}% soh={soh}% vmin={vmin_mv}mV vmax={vmax_mv}mV "
                    "ichg={ichg_a10} idsg={idsg_a10} cap={cap_now_ah100}/{cap_full_ah100} "
                    "fault={fault1:04X}/{fault2:04X}/{fault3:04X}".format(**row)
                )
                if index + 1 < args.samples:
                    time.sleep(args.interval)
    finally:
        if csv_file is not None:
            csv_file.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
