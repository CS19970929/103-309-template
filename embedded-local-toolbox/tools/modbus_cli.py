#!/usr/bin/env python3
"""Minimal Modbus RTU CLI. Write operations are dry-run unless --execute is set."""

from __future__ import annotations

import argparse
import time

from elt_common import format_hex, md_table, parse_int, write_text_report


def main() -> int:
    parser = argparse.ArgumentParser(description="串口 Modbus RTU 读写调试；写操作默认 dry-run")
    parser.add_argument("--port", help="串口号，例如 COM4 或 /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=19200, help="波特率")
    parser.add_argument("--slave", type=int, default=1, help="Modbus 从站地址")
    parser.add_argument("--read", nargs=2, metavar=("ADDR", "COUNT"), help="读保持寄存器")
    parser.add_argument("--write", nargs="+", metavar=("ADDR", "VALUE"), help="写一个或多个寄存器")
    parser.add_argument("--execute", action="store_true", help="真正执行写操作；默认只输出 dry-run 报告")
    parser.add_argument("--demo", action="store_true", help="使用内置离线示例，不访问串口")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有报告文件")
    args = parser.parse_args()

    report = run_cli(args)
    write_text_report(args.out, report, args.force)
    return 0


def run_cli(args: argparse.Namespace) -> str:
    out = ["# Modbus CLI 报告\n"]
    out.append(f"- slave: `{args.slave}`\n- baud: `{args.baud}`\n- mode: `{'demo' if args.demo else 'serial'}`\n")
    if args.read:
        addr = parse_int(args.read[0])
        count = parse_int(args.read[1])
        if args.demo or not args.port:
            values = [(addr + i) & 0xFFFF for i in range(count)]
            out.append("## 读寄存器结果\n")
            out.append(md_table(["地址", "值"], [(format_hex(addr + i), value) for i, value in enumerate(values)]))
        else:
            values = read_holding_registers(args.port, args.baud, args.slave, addr, count)
            out.append("## 读寄存器结果\n")
            out.append(md_table(["地址", "值"], [(format_hex(addr + i), value) for i, value in enumerate(values)]))
    if args.write:
        addr = parse_int(args.write[0])
        values = [parse_int(v) for v in args.write[1:]]
        if not values:
            raise SystemExit("--write 需要至少一个 VALUE")
        out.append("## 写寄存器计划\n")
        out.append(md_table(["地址", "值"], [(format_hex(addr + i), value) for i, value in enumerate(values)]))
        if not args.execute:
            out.append("\n状态：dry-run，未写入设备。\n")
        elif args.demo or not args.port:
            out.append("\n状态：demo 模式，未访问串口。\n")
        else:
            write_registers(args.port, args.baud, args.slave, addr, values)
            out.append("\n状态：已执行写入。\n")
    if not args.read and not args.write:
        out.append("\n未指定读写操作。使用 `--help` 查看参数。\n")
    return "\n".join(out)


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def with_crc(frame: bytes) -> bytes:
    crc = crc16(frame)
    return frame + bytes([crc & 0xFF, crc >> 8])


def read_holding_registers(port: str, baud: int, slave: int, addr: int, count: int) -> list[int]:
    serial = import_pyserial()
    frame = with_crc(bytes([slave, 0x03, addr >> 8, addr & 0xFF, count >> 8, count & 0xFF]))
    with serial.Serial(port, baudrate=baud, timeout=1.0) as ser:
        ser.write(frame)
        time.sleep(0.05)
        resp = ser.read(5 + count * 2)
    validate_response_crc(resp)
    if len(resp) < 5 or resp[1] & 0x80:
        raise RuntimeError(f"Modbus read failed: {resp.hex()}")
    return [(resp[3 + i * 2] << 8) | resp[4 + i * 2] for i in range(count)]


def write_registers(port: str, baud: int, slave: int, addr: int, values: list[int]) -> None:
    serial = import_pyserial()
    if len(values) == 1:
        value = values[0]
        frame = with_crc(bytes([slave, 0x06, addr >> 8, addr & 0xFF, value >> 8, value & 0xFF]))
        expected = 8
    else:
        payload = []
        for value in values:
            payload.extend([value >> 8, value & 0xFF])
        frame = with_crc(bytes([slave, 0x10, addr >> 8, addr & 0xFF, 0, len(values), len(payload)] + payload))
        expected = 8
    with serial.Serial(port, baudrate=baud, timeout=1.0) as ser:
        ser.write(frame)
        time.sleep(0.05)
        resp = ser.read(expected)
    validate_response_crc(resp)
    if len(resp) < expected or resp[1] & 0x80:
        raise RuntimeError(f"Modbus write failed: {resp.hex()}")


def validate_response_crc(resp: bytes) -> None:
    if len(resp) < 4:
        raise RuntimeError("Modbus response too short")
    got = resp[-2] | (resp[-1] << 8)
    expected = crc16(resp[:-2])
    if got != expected:
        raise RuntimeError(f"CRC mismatch: got 0x{got:04X}, expected 0x{expected:04X}")


def import_pyserial():
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise RuntimeError("pyserial 未安装；离线模式可使用 --demo") from exc
    return serial


if __name__ == "__main__":
    raise SystemExit(main())
