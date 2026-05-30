#!/usr/bin/env python3
"""Read-only OpenOCD/STLink probe helper. No erase/program commands are used."""

from __future__ import annotations

import argparse

from elt_common import command_exists, md_table, run_command, write_text_report


IDCODE_ADDR = "0xE0042000"


def main() -> int:
    parser = argparse.ArgumentParser(description="检查 OpenOCD/STLink 并可选只读读取芯片 ID；不执行擦写")
    parser.add_argument("--openocd", default="openocd", help="OpenOCD 可执行文件")
    parser.add_argument("--interface", default="interface/stlink.cfg", help="OpenOCD interface cfg")
    parser.add_argument("--target", default="target/stm32f1x.cfg", help="OpenOCD target cfg")
    parser.add_argument("--connect", action="store_true", help="连接目标并执行只读 IDCODE 读取；默认只检查 openocd 是否存在")
    parser.add_argument("--timeout", type=float, default=8.0, help="OpenOCD 超时时间")
    parser.add_argument("--demo-output", default="data/examples/openocd_probe_output.txt", help="离线示例输出")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖报告")
    args = parser.parse_args()

    report = probe(args)
    write_text_report(args.out, report, args.force)
    return 0


def probe(args: argparse.Namespace) -> str:
    found = command_exists(args.openocd)
    rows = [("openocd_path", found or "not found"), ("interface", args.interface), ("target", args.target), ("connect", args.connect)]
    output = ""
    if args.connect:
        if not found:
            output = "openocd not found"
        else:
            cmd = [args.openocd, "-f", args.interface, "-f", args.target, "-c", "init; mdw 0xE0042000 1; shutdown"]
            result = run_command(cmd, timeout=args.timeout)
            output = (result.stdout + "\n" + result.stderr).strip()
            rows.append(("returncode", result.returncode))
    else:
        output = "未传 --connect，未访问硬件。"
        if args.demo_output:
            output += "\n\n离线示例：\n" + open(args.demo_output, "r", encoding="utf-8").read()
    chip_id = extract_idcode(output)
    if chip_id:
        rows.append(("DBGMCU_IDCODE", chip_id))
    out = ["# OpenOCD/STLink 只读探测报告\n"]
    out.append("安全边界：本工具不执行 erase/program/reset，不自动烧录。\n")
    out.append(md_table(["项目", "结果"], rows))
    out.append("## OpenOCD 输出\n\n```text\n" + output + "\n```\n")
    return "\n".join(out)


def extract_idcode(output: str) -> str:
    for line in output.splitlines():
        if IDCODE_ADDR.lower() in line.lower() and ":" in line:
            return line.split(":", 1)[1].strip().split()[0]
    return ""


if __name__ == "__main__":
    raise SystemExit(main())
