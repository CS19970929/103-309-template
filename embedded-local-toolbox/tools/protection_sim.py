#!/usr/bin/env python3
"""Minimal BMS protection logic simulator."""

from __future__ import annotations

import argparse

from elt_common import md_table, read_csv, read_json, write_text_report


def main() -> int:
    parser = argparse.ArgumentParser(description="BMS 保护逻辑 PC 仿真，输出保护事件时间线")
    parser.add_argument("--params", default="data/examples/protection_params.json", help="保护参数 JSON")
    parser.add_argument("--series", default="data/examples/protection_timeseries.csv", help="时间序列 CSV")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有报告文件")
    args = parser.parse_args()
    params = read_json(args.params)
    series = read_csv(args.series)
    events = simulate(params, series)
    write_text_report(args.out, render_report(events), args.force)
    return 0


def simulate(params: dict, series: list[dict]) -> list[dict]:
    active = set()
    events = []
    rules = [
        ("CELL_OVP", "cell_mv", ">", params["cell_ovp_mv"]),
        ("CELL_UVP", "cell_mv", "<", params["cell_uvp_mv"]),
        ("CHG_OCP", "current_a", ">", params["chg_ocp_a"]),
        ("DSG_OCP", "current_a", "<", -abs(params["dsg_ocp_a"])),
        ("PACK_OTP", "temp_c", ">", params["pack_otp_c"]),
    ]
    for row in series:
        ts = float(row["time_s"])
        for name, field, op, threshold in rules:
            value = float(row[field])
            triggered = value > threshold if op == ">" else value < threshold
            if triggered and name not in active:
                active.add(name)
                events.append({"time_s": ts, "event": name, "action": "SET", "value": value, "threshold": threshold})
            elif not triggered and name in active:
                active.remove(name)
                events.append({"time_s": ts, "event": name, "action": "CLEAR", "value": value, "threshold": threshold})
    return events


def render_report(events: list[dict]) -> str:
    out = ["# BMS 保护仿真报告\n"]
    out.append(md_table(["时间(s)", "事件", "动作", "值", "阈值"], [(e["time_s"], e["event"], e["action"], e["value"], e["threshold"]) for e in events]) if events else "未触发保护事件。\n")
    return "\n".join(out)


if __name__ == "__main__":
    raise SystemExit(main())
