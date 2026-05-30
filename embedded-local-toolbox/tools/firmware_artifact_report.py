#!/usr/bin/env python3
"""Generate Markdown release reports for hex/bin/elf/map firmware artifacts."""

from __future__ import annotations

import argparse
import datetime as dt
import re
from pathlib import Path

from elt_common import crc32_file, file_sha256, md_table, run_command, write_text_report
from map_analyze import analyze_map


def main() -> int:
    parser = argparse.ArgumentParser(description="根据 hex/bin/elf/map 生成固件发布报告，包含版本、大小、CRC、构建时间")
    parser.add_argument("artifacts", nargs="*", default=["data/examples/firmware_demo.bin", "data/examples/firmware_demo.hex", "data/examples/example_keil.map"], help="固件产物路径")
    parser.add_argument("--version", help="固件版本；不提供时从文件名尝试提取")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖报告")
    args = parser.parse_args()
    write_text_report(args.out, render_report(args), args.force)
    return 0


def render_report(args: argparse.Namespace) -> str:
    rows = []
    map_rows = []
    for item in args.artifacts:
        path = Path(item)
        if not path.exists():
            rows.append((item, "missing", "-", "-", "-", "-"))
            continue
        stat = path.stat()
        crc = crc32_file(path)
        sha = file_sha256(path)[:16]
        build_time = dt.datetime.fromtimestamp(stat.st_mtime).isoformat(timespec="seconds")
        rows.append((str(path), path.suffix.lower() or "-", stat.st_size, f"0x{crc:08X}", sha, build_time))
        if path.suffix.lower() == ".map":
            info = analyze_map(path)
            summary = info["summary"]
            map_rows.append((str(path), summary["code"], summary["ro"], summary["rw"], summary["zi"], summary["code"] + summary["ro"] + summary["rw"], summary["rw"] + summary["zi"]))
        elif path.suffix.lower() == ".elf":
            map_rows.extend(read_elf_size(path))
    version = args.version or infer_version(args.artifacts)
    out = ["# 固件发布产物报告\n"]
    out.append(f"- version: `{version}`\n- generated_at: `{dt.datetime.now().isoformat(timespec='seconds')}`\n")
    out.append("## 产物清单\n")
    out.append(md_table(["文件", "类型", "大小(bytes)", "CRC32", "SHA256(前16)", "构建时间"], rows))
    if map_rows:
        out.append("## Flash/RAM 估算\n")
        out.append(md_table(["来源", "Code", "RO", "RW", "ZI", "Flash估算", "RAM估算"], map_rows))
    out.append("## 安全说明\n")
    out.append("- 本工具只读取本地产物并计算校验值，不烧录、不擦除、不连接设备。\n")
    return "\n".join(out)


def infer_version(paths: list[str]) -> str:
    for path in paths:
        match = re.search(r"v?(\d+\.\d+(?:\.\d+)?)", Path(path).name)
        if match:
            return match.group(1)
    return "unknown"


def read_elf_size(path: Path) -> list[tuple]:
    result = run_command(["size", str(path)], timeout=5)
    if result.returncode != 0:
        return []
    lines = [line.split() for line in result.stdout.splitlines() if line.strip()]
    if len(lines) < 2 or len(lines[1]) < 4:
        return []
    text, data, bss = int(lines[1][0]), int(lines[1][1]), int(lines[1][2])
    return [(str(path), text, 0, data, bss, text + data, data + bss)]


if __name__ == "__main__":
    raise SystemExit(main())
