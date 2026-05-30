#!/usr/bin/env python3
"""Generate embedded project change logs from git diff."""

from __future__ import annotations

import argparse
import subprocess
from collections import Counter

from elt_common import md_table, write_text_report


def main() -> int:
    parser = argparse.ArgumentParser(description="根据 git diff 生成嵌入式项目变更记录 Markdown")
    parser.add_argument("--repo", default=".", help="Git 仓库路径")
    parser.add_argument("--base", default="HEAD", help="对比基准，默认 HEAD")
    parser.add_argument("--staged", action="store_true", help="使用 staged diff")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有报告文件")
    args = parser.parse_args()
    diff = get_diff(args.repo, args.base, args.staged)
    write_text_report(args.out, render_change_log(diff, args), args.force)
    return 0


def get_diff(repo: str, base: str, staged: bool) -> str:
    cmd = ["git", "-C", repo, "diff"]
    if staged:
        cmd.append("--cached")
    else:
        cmd.append(base)
    result = subprocess.run(cmd, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        raise SystemExit(result.stderr.strip())
    return result.stdout


def render_change_log(diff: str, args: argparse.Namespace) -> str:
    files = []
    counters = Counter()
    current = None
    for line in diff.splitlines():
        if line.startswith("diff --git "):
            current = line.split()[-1][2:]
            files.append(current)
        elif current and line.startswith("+") and not line.startswith("+++"):
            counters[(current, "新增行")] += 1
        elif current and line.startswith("-") and not line.startswith("---"):
            counters[(current, "删除行")] += 1
    rows = []
    for file in files:
        rows.append((file, counters.get((file, "新增行"), 0), counters.get((file, "删除行"), 0), classify_file(file)))
    out = [f"# 嵌入式项目变更记录\n\n- repo: `{args.repo}`\n- base: `{args.base}`\n- staged: `{args.staged}`\n"]
    out.append("## 文件变更\n")
    out.append(md_table(["文件", "新增行", "删除行", "分类"], rows) if rows else "未发现 diff。\n")
    out.append("## 回归建议\n")
    out.append("- 通信协议、参数表、Flash/IAP、低功耗、中断和 MOS 控制相关变更需要优先做板端回归。\n")
    out.append("- 本工具只读取本地 git diff，不联网，不上传项目数据。\n")
    return "\n".join(out)


def classify_file(path: str) -> str:
    lower = path.lower()
    if any(k in lower for k in ["modbus", "can", "uart", "sci"]):
        return "通信"
    if any(k in lower for k in ["flash", "eeprom", "iap", "boot"]):
        return "存储/IAP"
    if any(k in lower for k in ["soc", "soh"]):
        return "SOC"
    if any(k in lower for k in ["mos", "protect", "fault"]):
        return "保护/MOS"
    if lower.endswith((".c", ".h")):
        return "源码"
    if lower.endswith((".md", ".txt")):
        return "文档"
    return "其他"


if __name__ == "__main__":
    raise SystemExit(main())
