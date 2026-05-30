#!/usr/bin/env python3
"""Decode BMS event logs with JSON event-code configuration."""

from __future__ import annotations

import argparse

from elt_common import md_table, parse_int, read_json, write_text_report


def main() -> int:
    parser = argparse.ArgumentParser(description="解析 BMS 事件日志并输出 Markdown")
    parser.add_argument("--events", default="data/examples/bms_events.json", help="事件码 JSON")
    parser.add_argument("--log", default="data/examples/bms_event_log.csv", help="事件日志 CSV")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有报告文件")
    args = parser.parse_args()
    events = read_json(args.events).get("events", {})
    rows = decode_log(args.log, events)
    write_text_report(args.out, render_decode(rows), args.force)
    return 0


def decode_log(path: str, events: dict) -> list[dict]:
    import csv

    rows = []
    with open(path, "r", encoding="utf-8-sig", newline="") as fp:
        for row in csv.DictReader(fp):
            code = parse_int(row["event_code"])
            info = events.get(f"0x{code:04X}", {})
            rows.append({"time": row.get("time", ""), "code": f"0x{code:04X}", "name": info.get("name", "UNKNOWN"), "level": info.get("level", "-"), "raw": row.get("raw", "")})
    return rows


def render_decode(rows: list[dict]) -> str:
    return "# BMS 事件日志解码报告\n\n" + md_table(["时间", "事件码", "名称", "等级", "原始数据"], [(r["time"], r["code"], r["name"], r["level"], r["raw"]) for r in rows])


if __name__ == "__main__":
    raise SystemExit(main())
