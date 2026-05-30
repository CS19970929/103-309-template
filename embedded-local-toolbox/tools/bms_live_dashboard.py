#!/usr/bin/env python3
"""Command-line BMS live dashboard using Modbus read windows or offline values."""

from __future__ import annotations

import argparse
import os
import time

from elt_common import md_table, parse_int, read_csv, write_text_report
from modbus_cli import read_holding_registers


def main() -> int:
    parser = argparse.ArgumentParser(description="串口读取 BMS 基本信息并周期刷新；只输出命令行表格")
    parser.add_argument("--table", default="data/examples/bms_dashboard_registers.csv", help="Dashboard 寄存器表 CSV")
    parser.add_argument("--values", default="data/examples/bms_dashboard_values.csv", help="离线寄存器值 CSV")
    parser.add_argument("--port", help="串口号；不提供时使用离线 values")
    parser.add_argument("--baud", type=int, default=19200, help="波特率")
    parser.add_argument("--slave", type=int, default=1, help="Modbus 从站地址")
    parser.add_argument("--interval", type=float, default=1.0, help="刷新周期，秒")
    parser.add_argument("--cycles", type=int, default=1, help="刷新次数")
    parser.add_argument("--out", help="保存最后一帧 Markdown 快照")
    parser.add_argument("--force", action="store_true", help="允许覆盖输出报告")
    args = parser.parse_args()

    last_rows = []
    for _ in range(max(1, args.cycles)):
        last_rows = read_dashboard(args)
        print_dashboard(last_rows)
        if args.cycles > 1:
            time.sleep(args.interval)
    if args.out:
        write_text_report(args.out, "# BMS Live Dashboard 快照\n\n" + md_table(["项目", "地址", "Raw", "Value", "Unit"], last_rows), args.force)
    return 0


def read_dashboard(args: argparse.Namespace) -> list[tuple]:
    table = read_csv(args.table)
    offline = {parse_int(row["addr"]): parse_int(row["value"]) for row in read_csv(args.values)} if not args.port else {}
    rows = []
    for item in table:
        addr = parse_int(item["addr"])
        raw = read_holding_registers(args.port, args.baud, args.slave, addr, 1)[0] if args.port else offline.get(addr, 0)
        value = raw * float(item.get("scale", "1") or "1") + float(item.get("offset", "0") or "0")
        rows.append((item["name"], f"0x{addr:04X}", raw, f"{value:g}", item.get("unit", "")))
    return rows


def print_dashboard(rows: list[tuple]) -> None:
    if os.environ.get("TERM"):
        print("\033[2J\033[H", end="")
    print("BMS Live Dashboard")
    print(md_table(["项目", "地址", "Raw", "Value", "Unit"], rows))


if __name__ == "__main__":
    raise SystemExit(main())
