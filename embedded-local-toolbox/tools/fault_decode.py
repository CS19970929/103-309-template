#!/usr/bin/env python3
"""Decode Cortex-M fault register snapshots and map PC/LR to nearest symbols."""

from __future__ import annotations

import argparse
from pathlib import Path

from elt_common import md_table, parse_int, read_json, write_text_report
from map_analyze import analyze_map


CFSR_BITS = {
    0: "IACCVIOL", 1: "DACCVIOL", 3: "MUNSTKERR", 4: "MSTKERR", 7: "MMARVALID",
    8: "IBUSERR", 9: "PRECISERR", 10: "IMPRECISERR", 11: "UNSTKERR", 12: "STKERR", 15: "BFARVALID",
    16: "UNDEFINSTR", 17: "INVSTATE", 18: "INVPC", 19: "NOCP", 24: "UNALIGNED", 25: "DIVBYZERO",
}


def main() -> int:
    parser = argparse.ArgumentParser(description="Cortex-M HardFault 现场解析工具")
    parser.add_argument("--fault", default="data/examples/fault_snapshot.json", help="故障寄存器 JSON")
    parser.add_argument("--map", default="data/examples/example_keil.map", help="map 文件")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有报告文件")
    args = parser.parse_args()
    fault = read_json(args.fault)
    info = analyze_map(Path(args.map))
    report = render_fault(fault, info)
    write_text_report(args.out, report, args.force)
    return 0


def render_fault(fault: dict, info: dict) -> str:
    pc = parse_int(fault.get("pc", 0))
    lr = parse_int(fault.get("lr", 0))
    cfsr = parse_int(fault.get("cfsr", 0))
    rows = [(k, v) for k, v in fault.items()]
    bits = [name for bit, name in CFSR_BITS.items() if cfsr & (1 << bit)]
    out = ["# Cortex-M HardFault 解析报告\n"]
    out.append("## 寄存器\n")
    out.append(md_table(["寄存器", "值"], rows))
    out.append("## CFSR 位解析\n")
    out.append(md_table(["位含义"], [(b,) for b in bits]) if bits else "未解析到 CFSR 置位。\n")
    out.append("## PC/LR 符号定位\n")
    out.append(md_table(["寄存器", "地址", "最近符号"], [("PC", f"0x{pc:08X}", nearest_symbol(pc, info)), ("LR", f"0x{lr:08X}", nearest_symbol(lr, info))]))
    return "\n".join(out)


def nearest_symbol(addr: int, info: dict) -> str:
    candidates = [s for s in info["symbols"] if s["addr"] <= addr]
    if not candidates:
        return "未找到"
    sym = max(candidates, key=lambda s: s["addr"])
    return f"{sym['name']}+0x{addr - sym['addr']:X} ({sym['module']})"


if __name__ == "__main__":
    raise SystemExit(main())
