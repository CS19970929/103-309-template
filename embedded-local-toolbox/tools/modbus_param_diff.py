#!/usr/bin/env python3
"""Compare two Modbus parameter dump CSV files."""

from __future__ import annotations

import argparse

from elt_common import md_table, read_csv, write_text_report


def main() -> int:
    parser = argparse.ArgumentParser(description="对比两份 Modbus 参数导出 CSV 并输出 Markdown")
    parser.add_argument("old_dump", nargs="?", default="data/examples/modbus_dump_a.csv", help="旧参数 dump CSV")
    parser.add_argument("new_dump", nargs="?", default="data/examples/modbus_dump_b.csv", help="新参数 dump CSV")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有报告文件")
    args = parser.parse_args()
    old_rows = {row["name"]: row for row in read_csv(args.old_dump)}
    new_rows = {row["name"]: row for row in read_csv(args.new_dump)}
    changes = []
    for name in sorted(set(old_rows) | set(new_rows)):
        old = old_rows.get(name, {})
        new = new_rows.get(name, {})
        if old.get("raw") != new.get("raw") or old.get("value") != new.get("value"):
            changes.append((name, old.get("addr") or new.get("addr"), old.get("raw", "-"), new.get("raw", "-"), old.get("value", "-"), new.get("value", "-")))
    report = "# Modbus 参数差异报告\n\n" + (md_table(["参数", "地址", "旧 Raw", "新 Raw", "旧值", "新值"], changes) if changes else "未发现差异。\n")
    write_text_report(args.out, report, args.force)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
