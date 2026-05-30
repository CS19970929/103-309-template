#!/usr/bin/env python3
"""Generate Markdown parameter documents from CSV."""

from __future__ import annotations

import argparse
from pathlib import Path

from elt_common import md_table, parse_int, read_csv, write_text_report
from param_table_check import TYPES, check_rows


def main() -> int:
    parser = argparse.ArgumentParser(description="从参数表 CSV 生成参数表和 Modbus 文档")
    parser.add_argument("csv", nargs="?", default="data/examples/param_table.csv", help="参数表 CSV")
    parser.add_argument("--out-dir", default="generated", help="输出目录")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有生成文档")
    args = parser.parse_args()
    rows = read_csv(args.csv)
    errors = check_rows(rows)
    if errors:
        raise SystemExit("参数表检查失败，请先运行 param_table_check.py")
    out_dir = Path(args.out_dir)
    write_text_report(out_dir / "param_table.md", render_param_doc(rows, args.csv), args.force)
    write_text_report(out_dir / "modbus_register_map.md", render_modbus_doc(rows, args.csv), args.force)
    print(f"已生成到 {out_dir}")
    return 0


def render_param_doc(rows: list[dict], source: str) -> str:
    out = [f"# 参数表\n\n> 自动生成，请勿手动修改。源数据：`{source}`。\n\n"]
    out.append(md_table(["Group", "Name", "C name", "Type", "Scale", "Unit", "Min", "Max", "Default", "Access", "Save", "Description"], [(r["group"], r["name"], f"`{r['c_name']}`", r["data_type"], r["scale"], r["unit"], r["min"], r["max"], r["default"], r["access"], r["save_policy"], r["description"]) for r in rows]))
    return "".join(out)


def render_modbus_doc(rows: list[dict], source: str) -> str:
    out = [f"# Modbus 参数映射\n\n> 自动生成，请勿手动修改。源数据：`{source}`。\n\n"]
    out.append(md_table(["Address", "Regs", "C name", "Name", "Access", "Description"], [(f"`0x{parse_int(r['modbus_addr']):04X}`", TYPES[r["data_type"].lower()], f"`{r['c_name']}`", r["name"], r["access"], r["description"]) for r in rows]))
    return "".join(out)


if __name__ == "__main__":
    raise SystemExit(main())
