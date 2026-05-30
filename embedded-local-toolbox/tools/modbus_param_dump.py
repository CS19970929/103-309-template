#!/usr/bin/env python3
"""Dump Modbus parameters from CSV register configuration or offline values."""

from __future__ import annotations

import argparse
from pathlib import Path

from elt_common import md_table, parse_int, read_csv, write_csv, write_text_report
from modbus_cli import read_holding_registers


def main() -> int:
    parser = argparse.ArgumentParser(description="按 CSV 寄存器表导出 Modbus 参数；无硬件可用 --values 离线自测")
    parser.add_argument("--table", default="data/examples/modbus_registers.csv", help="寄存器表 CSV")
    parser.add_argument("--values", default="data/examples/modbus_values_a.csv", help="离线寄存器值 CSV")
    parser.add_argument("--port", help="串口号；不提供时使用离线 values")
    parser.add_argument("--baud", type=int, default=19200, help="波特率")
    parser.add_argument("--slave", type=int, default=1, help="Modbus 从站地址")
    parser.add_argument("--csv-out", help="同时输出参数 dump CSV")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有输出文件")
    args = parser.parse_args()

    rows = dump_params(args)
    if args.csv_out:
        write_csv(args.csv_out, rows, ["name", "addr", "raw", "value", "unit", "access"], args.force)
    write_text_report(args.out, render_report(rows), args.force)
    return 0


def dump_params(args: argparse.Namespace) -> list[dict]:
    table = read_csv(args.table)
    offline_values = {parse_int(row["addr"]): parse_int(row["value"]) for row in read_csv(args.values)} if not args.port else {}
    result = []
    for row in table:
        addr = parse_int(row["addr"])
        if args.port:
            raw = read_holding_registers(args.port, args.baud, args.slave, addr, 1)[0]
        else:
            raw = offline_values.get(addr, 0)
        scale = float(row.get("scale", "1") or "1")
        value = raw * scale
        result.append({"name": row["name"], "addr": f"0x{addr:04X}", "raw": raw, "value": value, "unit": row.get("unit", ""), "access": row.get("access", "")})
    return result


def render_report(rows: list[dict]) -> str:
    out = ["# Modbus 参数导出报告\n"]
    out.append(md_table(["参数", "地址", "Raw", "Value", "Unit", "Access"], [(r["name"], r["addr"], r["raw"], r["value"], r["unit"], r["access"]) for r in rows]))
    return "\n".join(out)


if __name__ == "__main__":
    raise SystemExit(main())
