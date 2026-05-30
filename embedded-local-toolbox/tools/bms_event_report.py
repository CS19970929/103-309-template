#!/usr/bin/env python3
"""Summarize decoded BMS event logs into a Markdown report."""

from __future__ import annotations

import argparse
from collections import Counter

from bms_log_decode import decode_log
from elt_common import md_table, read_json, write_text_report


def main() -> int:
    parser = argparse.ArgumentParser(description="生成 BMS 事件统计报告")
    parser.add_argument("--events", default="data/examples/bms_events.json", help="事件码 JSON")
    parser.add_argument("--log", default="data/examples/bms_event_log.csv", help="事件日志 CSV")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有报告文件")
    args = parser.parse_args()
    events = read_json(args.events).get("events", {})
    rows = decode_log(args.log, events)
    code_counts = Counter((r["code"], r["name"], r["level"]) for r in rows)
    level_counts = Counter(r["level"] for r in rows)
    report = ["# BMS 事件统计报告\n"]
    report.append("## 等级统计\n")
    report.append(md_table(["等级", "次数"], level_counts.most_common()))
    report.append("## 事件统计\n")
    report.append(md_table(["事件码", "名称", "等级", "次数"], [(k[0], k[1], k[2], v) for k, v in code_counts.most_common()]))
    write_text_report(args.out, "\n".join(report), args.force)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
