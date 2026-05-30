#!/usr/bin/env python3
"""Read STM32 flash-size register through OpenOCD in read-only mode."""

from __future__ import annotations

import argparse
import re

from elt_common import command_exists, md_table, run_command, write_text_report


FLASH_SIZE_ADDR = {
    "stm32f0": "0x1FFFF7CC",
    "stm32f1": "0x1FFFF7E0",
    "stm32f4": "0x1FFF7A22",
}
TARGET_CFG = {
    "stm32f0": "target/stm32f0x.cfg",
    "stm32f1": "target/stm32f1x.cfg",
    "stm32f4": "target/stm32f4x.cfg",
}


def main() -> int:
    parser = argparse.ArgumentParser(description="读取 STM32 Flash size 寄存器并生成报告；不擦写")
    parser.add_argument("--mcu", choices=sorted(FLASH_SIZE_ADDR), default="stm32f1", help="MCU 系列")
    parser.add_argument("--openocd", default="openocd", help="OpenOCD 可执行文件")
    parser.add_argument("--interface", default="interface/stlink.cfg", help="OpenOCD interface cfg")
    parser.add_argument("--target", help="OpenOCD target cfg；默认按 --mcu 选择")
    parser.add_argument("--connect", action="store_true", help="连接目标并只读 flash size；默认使用 demo 输出")
    parser.add_argument("--timeout", type=float, default=8.0, help="OpenOCD 超时时间")
    parser.add_argument("--demo-output", default="data/examples/stlink_flash_size_output.txt", help="离线示例输出")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖报告")
    args = parser.parse_args()
    write_text_report(args.out, render_report(args), args.force)
    return 0


def render_report(args: argparse.Namespace) -> str:
    target = args.target or TARGET_CFG[args.mcu]
    addr = FLASH_SIZE_ADDR[args.mcu]
    found = command_exists(args.openocd)
    if args.connect and found:
        cmd = [args.openocd, "-f", args.interface, "-f", target, "-c", f"init; mdw {addr} 1; shutdown"]
        result = run_command(cmd, timeout=args.timeout)
        output = (result.stdout + "\n" + result.stderr).strip()
        returncode = result.returncode
    elif args.connect:
        output = "openocd not found"
        returncode = "not-run"
    else:
        output = open(args.demo_output, "r", encoding="utf-8").read()
        returncode = "demo"
    kb = extract_flash_kb(output)
    rows = [
        ("mcu", args.mcu),
        ("flash_size_addr", addr),
        ("openocd_path", found or "not found"),
        ("connect", args.connect),
        ("returncode", returncode),
        ("flash_size_kb", kb or "unknown"),
    ]
    out = ["# STLink Flash Size 只读检查报告\n"]
    out.append("安全边界：本工具只读 flash size 系统存储器地址，不执行 erase/program/reset。\n")
    out.append(md_table(["项目", "结果"], rows))
    out.append("## 原始输出\n\n```text\n" + output + "\n```\n")
    return "\n".join(out)


def extract_flash_kb(output: str) -> str:
    match = re.search(r"0x[0-9A-Fa-f]+:\s+0x([0-9A-Fa-f]+)", output)
    if not match:
        return ""
    value = int(match.group(1), 16)
    kb = value & 0xFFFF
    return str(kb)


if __name__ == "__main__":
    raise SystemExit(main())
