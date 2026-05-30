#!/usr/bin/env python3
"""Scan embedded C projects and generate a static risk Markdown report."""

from __future__ import annotations

import argparse
import re
from collections import Counter, defaultdict
from pathlib import Path

from elt_common import find_text_files, md_table, rel, strip_comments, write_text_report


C_SUFFIXES = [".c", ".h"]
FUNC_RE = re.compile(
    r"^\s*(?:static\s+|inline\s+|extern\s+|__weak\s+|__attribute__\s*\(\([^)]*\)\)\s*)*"
    r"[A-Za-z_][\w\s\*]+\s+([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{",
    re.M,
)
ISR_RE = re.compile(r"\b([A-Za-z_]\w*(?:IRQHandler|IRQ_Handler|ISR))\s*\(")
GLOBAL_RE = re.compile(r"^\s*(?:volatile\s+)?(?:static\s+)?(?:const\s+)?(?:u?int(?:8|16|32|64)_t|int|char|float|double|bool|uint8|uint16|uint32|uint64)\s+([A-Za-z_]\w*)\s*(?:=|\[|;)", re.M)
VOLATILE_RE = re.compile(r"\bvolatile\b[^;\n]*\b([A-Za-z_]\w*)\b[^;\n]*;")
MACRO_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)", re.M)
WHILE_WAIT_RE = re.compile(r"\bwhile\s*\([^)]*\)\s*(?:;|\{\s*\})")


def main() -> int:
    parser = argparse.ArgumentParser(description="扫描嵌入式 C 项目并输出静态风险 Markdown 报告")
    parser.add_argument("project", nargs="?", default="data/examples/example_c_project", help="项目根目录")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有报告文件")
    parser.add_argument("--long-func-lines", type=int, default=120, help="超长函数行数阈值")
    args = parser.parse_args()

    root = Path(args.project)
    files = find_text_files(root, C_SUFFIXES)
    report = analyze_project(root, files, args.long_func_lines)
    write_text_report(args.out, report, args.force)
    return 0


def analyze_project(root: Path, files: list[Path], long_func_lines: int) -> str:
    stats = Counter(path.suffix.lower() for path in files)
    total_lines = 0
    findings = defaultdict(list)
    functions = []
    macros = Counter()

    for path in files:
        text = path.read_text(encoding="utf-8", errors="ignore")
        code = strip_comments(text)
        lines = text.splitlines()
        total_lines += len(lines)
        rpath = rel(path, root)

        for match in FUNC_RE.finditer(code):
            name = match.group(1)
            start_line = code[: match.start()].count("\n") + 1
            end_line = estimate_function_end(code, match.end())
            line_count = max(1, end_line - start_line + 1)
            functions.append((name, rpath, start_line, line_count))
            if line_count >= long_func_lines:
                findings["超长函数"].append((rpath, start_line, name, f"{line_count} 行"))

        for name in ISR_RE.findall(code):
            findings["中断函数"].append((rpath, "-", name, classify_isr(name)))
        top_code = top_level_code(code)
        for name in GLOBAL_RE.findall(top_code):
            findings["全局变量候选"].append((rpath, "-", name, "需人工确认作用域"))
        for name in VOLATILE_RE.findall(code):
            findings["volatile 变量"].append((rpath, "-", name, "可能跨中断/主循环共享"))
        for line_no, line in enumerate(lines, 1):
            upper = line.upper()
            if "TODO" in upper or "FIXME" in upper:
                findings["TODO/FIXME"].append((rpath, line_no, "-", line.strip()))
            if "while (0)" not in line and "while(0)" not in line and (WHILE_WAIT_RE.search(line) or re.search(r"\bwhile\s*\(", line)):
                findings["while 死等"].append((rpath, line_no, "-", line.strip()))
            if not is_function_prototype(line) and re.search(r"\b(?:[A-Za-z_]*Delay[A-Za-z_]*|[A-Za-z_]*delay[A-Za-z_]*)\s*\(", line):
                findings["delay 调用"].append((rpath, line_no, "-", line.strip()))
            if re.search(r"\bHAL_[A-Za-z0-9_]+\s*\(", line):
                findings["HAL 调用"].append((rpath, line_no, "-", line.strip()))
            if not is_function_prototype(line) and re.search(r"\b(?:FLASH_|Flash|flash_)[A-Za-z0-9_]*(?:Write|Program|Erase|write|program|erase)[A-Za-z0-9_]*\s*\(", line):
                findings["Flash 写入调用"].append((rpath, line_no, "-", line.strip()))
            if re.search(r"\b(MOS|Mos|mos|CHG|DSG|ChargeMos|DischargeMos)", line):
                findings["MOS 控制相关"].append((rpath, line_no, "-", line.strip()))
            if re.search(r"\b(memcpy|memset|strcpy|sprintf|vsprintf|strcat)\s*\(", line):
                findings["风险库函数"].append((rpath, line_no, "-", line.strip()))
        macros.update(MACRO_RE.findall(code))

    out = ["# 嵌入式项目静态风险报告\n"]
    out.append("## 文件统计\n")
    out.append(md_table(["类型", "数量"], [(k, v) for k, v in sorted(stats.items())] + [("总行数", total_lines)]))
    out.append("## 函数列表\n")
    out.append(md_table(["函数", "文件", "起始行", "估算行数"], sorted(functions, key=lambda x: (x[1], x[2]))[:300]))
    for title in [
        "中断函数", "全局变量候选", "volatile 变量", "TODO/FIXME", "while 死等", "delay 调用",
        "HAL 调用", "Flash 写入调用", "MOS 控制相关", "风险库函数", "超长函数",
    ]:
        out.append(f"## {title}\n")
        rows = findings.get(title, [])
        out.append(md_table(["文件", "行", "对象", "说明"], rows[:300]) if rows else "未发现。\n")
    out.append("## UART/CAN/I2C/SPI 中断函数\n")
    periph = [row for row in findings.get("中断函数", []) if any(k in row[2].upper() for k in ["USART", "UART", "CAN", "I2C", "SPI"])]
    out.append(md_table(["文件", "行", "对象", "说明"], periph) if periph else "未发现。\n")
    out.append("## 宏开关统计\n")
    out.append(md_table(["宏", "出现次数"], macros.most_common(300)) if macros else "未发现。\n")
    return "\n".join(out)


def estimate_function_end(code: str, body_start: int) -> int:
    depth = 1
    index = body_start
    while index < len(code):
        ch = code[index]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return code[: index].count("\n") + 1
        index += 1
    return code.count("\n") + 1


def top_level_code(code: str) -> str:
    output = []
    depth = 0
    for line in code.splitlines():
        if depth == 0:
            output.append(line)
        depth += line.count("{")
        depth -= line.count("}")
        if depth < 0:
            depth = 0
    return "\n".join(output)


def is_function_prototype(line: str) -> bool:
    text = line.strip()
    return text.endswith(";") and re.match(r"^(?:void|int|char|float|double|bool|u?int(?:8|16|32|64)_t)\b", text) is not None


def classify_isr(name: str) -> str:
    upper = name.upper()
    tags = [tag for tag in ["UART", "USART", "CAN", "I2C", "SPI", "ADC", "TIM", "RTC", "EXTI"] if tag in upper]
    return ",".join(tags) if tags else "ISR"


if __name__ == "__main__":
    raise SystemExit(main())
