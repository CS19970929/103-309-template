#!/usr/bin/env python3
"""Analyze offline CAN frame periods and counts."""

from __future__ import annotations

import argparse
from collections import defaultdict

from can_decode import load_can_log
from elt_common import format_hex, md_table, write_text_report


def main() -> int:
    parser = argparse.ArgumentParser(description="离线 CAN 日志周期分析")
    parser.add_argument("--log", default="data/examples/can_log.txt", help="CAN 日志文本")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有报告文件")
    args = parser.parse_args()
    frames = load_can_log(args.log)
    write_text_report(args.out, render_period_report(frames), args.force)
    return 0


def render_period_report(frames: list[dict]) -> str:
    buckets = defaultdict(list)
    for frame in frames:
        buckets[frame["id"]].append(frame["ts"])
    rows = []
    for can_id, times in sorted(buckets.items()):
        periods = [round(times[i] - times[i - 1], 6) for i in range(1, len(times))]
        rows.append((format_hex(can_id, 3), len(times), min(periods) if periods else "-", max(periods) if periods else "-", round(sum(periods) / len(periods), 6) if periods else "-"))
    return "# CAN 帧周期分析报告\n\n" + md_table(["CAN ID", "帧数", "最小周期(s)", "最大周期(s)", "平均周期(s)"], rows)


if __name__ == "__main__":
    raise SystemExit(main())
