#!/usr/bin/env python3
"""Diff two map files and output Markdown size changes."""

from __future__ import annotations

import argparse
from pathlib import Path

from elt_common import md_table, write_text_report
from map_analyze import analyze_map


def main() -> int:
    parser = argparse.ArgumentParser(description="对比两次 Keil map 文件的 Flash/RAM/符号差异")
    parser.add_argument("old_map", nargs="?", default="data/examples/example_keil_old.map", help="旧 map 文件")
    parser.add_argument("new_map", nargs="?", default="data/examples/example_keil.map", help="新 map 文件")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有报告文件")
    parser.add_argument("--top", type=int, default=30, help="输出变化最大的符号数量")
    args = parser.parse_args()
    old_info = analyze_map(Path(args.old_map))
    new_info = analyze_map(Path(args.new_map))
    write_text_report(args.out, render_diff(args.old_map, args.new_map, old_info, new_info, args.top), args.force)
    return 0


def render_diff(old_source: str, new_source: str, old_info: dict, new_info: dict, top: int) -> str:
    rows = []
    for key, label in [("code", "Code"), ("ro", "RO-data"), ("rw", "RW-data"), ("zi", "ZI-data")]:
        old = old_info["summary"][key]
        new = new_info["summary"][key]
        rows.append((label, old, new, new - old))
    old_symbols = {s["name"]: s for s in old_info["symbols"]}
    new_symbols = {s["name"]: s for s in new_info["symbols"]}
    changes = []
    for name in sorted(set(old_symbols) | set(new_symbols)):
        old_size = old_symbols.get(name, {}).get("size", 0)
        new_size = new_symbols.get(name, {}).get("size", 0)
        delta = new_size - old_size
        if delta:
            module = new_symbols.get(name, old_symbols.get(name, {})).get("module", "-")
            changes.append((name, old_size, new_size, delta, module))
    changes.sort(key=lambda row: abs(row[3]), reverse=True)

    out = [f"# Map 差异报告\n\n旧文件：`{old_source}`\n\n新文件：`{new_source}`\n"]
    out.append("## 总览差异\n")
    out.append(md_table(["区域", "旧值", "新值", "变化"], rows))
    out.append("## 符号变化\n")
    out.append(md_table(["符号", "旧大小", "新大小", "变化", "模块"], changes[:top]) if changes else "未发现符号大小变化。\n")
    return "\n".join(out)


if __name__ == "__main__":
    raise SystemExit(main())
