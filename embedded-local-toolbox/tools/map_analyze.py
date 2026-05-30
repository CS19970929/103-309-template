#!/usr/bin/env python3
"""Analyze Keil-like map files and output Markdown size reports."""

from __future__ import annotations

import argparse
import re
from collections import Counter, defaultdict
from pathlib import Path

from elt_common import md_table, write_text_report


PROGRAM_SIZE_RE = re.compile(r"Program Size:\s*Code=(\d+)\s*RO-data=(\d+)\s*RW-data=(\d+)\s*ZI-data=(\d+)")
SYMBOL_RE = re.compile(r"^\s*(0x[0-9A-Fa-f]+)\s+(\d+|0x[0-9A-Fa-f]+)\s+(Code|Data|ZI|RO|RW)\s+(\S+)\s+(\S+)")


def main() -> int:
    parser = argparse.ArgumentParser(description="解析 Keil map 文件并输出 Flash/RAM/符号/模块占用报告")
    parser.add_argument("map_file", nargs="?", default="data/examples/example_keil.map", help="map 文件路径")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有报告文件")
    parser.add_argument("--top", type=int, default=20, help="大函数/大变量输出数量")
    args = parser.parse_args()
    info = analyze_map(Path(args.map_file))
    write_text_report(args.out, render_report(args.map_file, info, args.top), args.force)
    return 0


def analyze_map(path: Path) -> dict:
    text = path.read_text(encoding="utf-8", errors="ignore")
    summary = {"code": 0, "ro": 0, "rw": 0, "zi": 0}
    match = PROGRAM_SIZE_RE.search(text)
    if match:
        summary = {"code": int(match.group(1)), "ro": int(match.group(2)), "rw": int(match.group(3)), "zi": int(match.group(4))}

    symbols = []
    modules = Counter()
    for line in text.splitlines():
        match = SYMBOL_RE.match(line)
        if not match:
            continue
        addr = int(match.group(1), 16)
        size = int(match.group(2), 0)
        kind = match.group(3)
        name = match.group(4)
        module = match.group(5)
        symbols.append({"addr": addr, "size": size, "kind": kind, "name": name, "module": module})
        modules[module] += size
    if not match and symbols:
        summary["code"] = sum(s["size"] for s in symbols if s["kind"] in ("Code", "RO"))
        summary["rw"] = sum(s["size"] for s in symbols if s["kind"] == "RW")
        summary["zi"] = sum(s["size"] for s in symbols if s["kind"] in ("Data", "ZI"))
    return {"summary": summary, "symbols": symbols, "modules": modules}


def render_report(source: str, info: dict, top: int = 20) -> str:
    summary = info["summary"]
    flash = summary["code"] + summary["ro"] + summary["rw"]
    ram = summary["rw"] + summary["zi"]
    code_symbols = [s for s in info["symbols"] if s["kind"] in ("Code", "RO")]
    data_symbols = [s for s in info["symbols"] if s["kind"] in ("Data", "RW", "ZI")]
    out = [f"# Map 占用分析报告\n\n源文件：`{source}`\n"]
    out.append("## 总览\n")
    out.append(md_table(["项目", "字节"], [("Code", summary["code"]), ("RO-data", summary["ro"]), ("RW-data", summary["rw"]), ("ZI-data", summary["zi"]), ("Flash 估算", flash), ("RAM 估算", ram)]))
    out.append("## 大函数/代码符号\n")
    out.append(md_table(["符号", "大小", "地址", "模块"], [(s["name"], s["size"], f"0x{s['addr']:08X}", s["module"]) for s in sorted(code_symbols, key=lambda x: x["size"], reverse=True)[:top]]) if code_symbols else "未解析到代码符号。\n")
    out.append("## 大变量/数据符号\n")
    out.append(md_table(["符号", "大小", "地址", "模块"], [(s["name"], s["size"], f"0x{s['addr']:08X}", s["module"]) for s in sorted(data_symbols, key=lambda x: x["size"], reverse=True)[:top]]) if data_symbols else "未解析到数据符号。\n")
    out.append("## 模块占用\n")
    out.append(md_table(["模块", "大小"], info["modules"].most_common(top)) if info["modules"] else "未解析到模块信息。\n")
    return "\n".join(out)


if __name__ == "__main__":
    raise SystemExit(main())
