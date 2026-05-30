#!/usr/bin/env python3
"""Minimal SOC coulomb-counting simulator."""

from __future__ import annotations

import argparse

from elt_common import md_table, read_csv, read_json, write_csv, write_text_report


def main() -> int:
    parser = argparse.ArgumentParser(description="SOC PC 仿真，输出 estimated_soc/display_soc/remaining_mah")
    parser.add_argument("--params", default="data/examples/soc_params.json", help="SOC 参数 JSON")
    parser.add_argument("--series", default="data/examples/soc_timeseries.csv", help="时间序列 CSV")
    parser.add_argument("--csv-out", help="可选输出 CSV")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有输出文件")
    args = parser.parse_args()
    rows = simulate(read_json(args.params), read_csv(args.series))
    if args.csv_out:
        write_csv(args.csv_out, rows, ["time_s", "voltage_mv", "current_a", "estimated_soc", "display_soc", "remaining_mah"], args.force)
    write_text_report(args.out, render_report(rows), args.force)
    return 0


def simulate(params: dict, series: list[dict]) -> list[dict]:
    capacity_mah = float(params["capacity_mah"])
    remaining = capacity_mah * float(params.get("initial_soc", 50)) / 100.0
    display_soc = float(params.get("initial_soc", 50))
    last_t = None
    result = []
    for row in series:
        ts = float(row["time_s"])
        current = float(row["current_a"])
        voltage = float(row["voltage_mv"])
        if last_t is not None:
            dt_h = (ts - last_t) / 3600.0
            remaining += current * 1000.0 * dt_h
            remaining = max(0.0, min(capacity_mah, remaining))
        estimated_soc = remaining * 100.0 / capacity_mah
        if abs(estimated_soc - display_soc) >= float(params.get("display_step_pct", 1)):
            display_soc += 1 if estimated_soc > display_soc else -1
        result.append({"time_s": ts, "voltage_mv": voltage, "current_a": current, "estimated_soc": round(estimated_soc, 3), "display_soc": round(display_soc, 3), "remaining_mah": round(remaining, 3)})
        last_t = ts
    return result


def render_report(rows: list[dict]) -> str:
    return "# SOC 仿真报告\n\n" + md_table(["时间(s)", "电压(mV)", "电流(A)", "estimated_soc", "display_soc", "remaining_mah"], [(r["time_s"], r["voltage_mv"], r["current_a"], r["estimated_soc"], r["display_soc"], r["remaining_mah"]) for r in rows])


if __name__ == "__main__":
    raise SystemExit(main())
