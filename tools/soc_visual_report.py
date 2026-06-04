#!/usr/bin/env python3
"""Build real SOC C trace data and render a standalone visual HTML report."""

from __future__ import annotations

import argparse
import csv
import html
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build" / "host_tests"
SOURCE_DIR = ROOT / "103 + 309" / "Project" / "Source"
STM32_LIB = ROOT / "103 + 309" / "Project" / "STM32F10x_StdPeriph_Lib_V3.5.0"


@dataclass
class ScenarioSummary:
    name: str
    duration_s: int
    true_start: float
    true_end: float
    internal_start: int
    internal_end: int
    public_end: int
    max_abs_error: float
    min_vmin: int
    max_current_a10: int
    passed: bool
    note: str


def compiler() -> str:
    env_cc = os.environ.get("CC")
    if env_cc:
        return env_cc
    for candidate in ("clang", "cc", "gcc"):
        found = shutil.which(candidate)
        if found:
            return found
    raise RuntimeError("no C compiler found; install clang/gcc or set CC")


def build_trace_exe(cc: str, exe: Path) -> None:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    cmd = [
        cc,
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-ffunction-sections",
        "-fdata-sections",
        "-DSTM32F10X_HD",
        "-DUSE_STDPERIPH_DRIVER",
        "-I" + str(SOURCE_DIR),
        "-I" + str(SOURCE_DIR / "conf"),
        "-I" + str(SOURCE_DIR / "easylogger" / "inc"),
        "-I" + str(ROOT / "103 + 309" / "Project" / "Users"),
        "-I" + str(STM32_LIB / "drivers"),
        "-I" + str(STM32_LIB / "inc"),
        "-I" + str(ROOT / "103 + 309" / "Libraries" / "CMSIS" / "CM3" / "CoreSupport"),
        "-I" + str(ROOT / "103 + 309" / "Libraries" / "CMSIS" / "CM3" / "DeviceSupport" / "ST" / "STM32F10x"),
        "-I" + str(ROOT / "103 + 309" / "Libraries" / "STM32F10x_StdPeriph_Driver" / "inc"),
        str(ROOT / "tools" / "soc_host_visual_trace.c"),
        str(SOURCE_DIR / "SOC.c"),
        str(SOURCE_DIR / "SocEnhance.c"),
        "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections",
        "-o",
        str(exe),
    ]
    print("Building SOC visual trace with:", cc, exe.name, flush=True)
    subprocess.run(cmd, cwd=ROOT, check=True)


def run_trace(exe: Path, csv_path: Path) -> None:
    print("Running", exe, flush=True)
    with csv_path.open("w", encoding="utf-8", newline="") as f:
        subprocess.run([str(exe)], cwd=ROOT, check=True, stdout=f)
    print("csv={0}".format(csv_path), flush=True)


def load_rows(csv_path: Path) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    with csv_path.open("r", encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            rows.append(
                {
                    "scenario": row["scenario"],
                    "time_s": int(row["time_s"]),
                    "segment": row["segment"],
                    "true_soc": float(row["true_soc"]),
                    "internal_soc": int(row["internal_soc"]),
                    "public_soc": int(row["public_soc"]),
                    "vmin_mv": int(row["vmin_mv"]),
                    "vmax_mv": int(row["vmax_mv"]),
                    "ichg_a10": int(row["ichg_a10"]),
                    "idsg_a10": int(row["idsg_a10"]),
                    "soh": int(row["soh"]),
                    "cap_now_ah100": int(row["cap_now_ah100"]),
                }
            )
    return rows


def group_rows(rows: Iterable[dict[str, object]]) -> dict[str, list[dict[str, object]]]:
    grouped: dict[str, list[dict[str, object]]] = {}
    for row in rows:
        grouped.setdefault(str(row["scenario"]), []).append(row)
    return grouped


def scenario_note(name: str) -> str:
    notes = {
        "city_commute": "城市混合骑行应跟随安时积分，对外 SOC 不应乱跳。",
        "hill_climb": "大电流爬坡产生压降，但不能误判为空电。",
        "fast_current_pulses": "快变电流按 200ms 采样平均值积分，曲线应平滑。",
        "deep_cutoff": "接近控制器截止电压时，SOC 应在低压末端收敛到 0。",
        "charge_anchor": "充电积分到 99 后等待高压锚点，确认后才发布 100。",
    }
    return notes.get(name, "")


def summarize(name: str, rows: list[dict[str, object]]) -> ScenarioSummary:
    first = rows[0]
    last = rows[-1]
    max_abs_error = max(abs(int(row["internal_soc"]) - float(row["true_soc"])) for row in rows)
    min_vmin = min(int(row["vmin_mv"]) for row in rows)
    max_current = max(max(int(row["ichg_a10"]), int(row["idsg_a10"])) for row in rows)
    internal_end = int(last["internal_soc"])
    public_end = int(last["public_soc"])

    if name == "city_commute":
        passed = max_abs_error <= 8.0 and 50 <= internal_end <= 75
    elif name == "hill_climb":
        passed = internal_end > 40 and max_abs_error <= 12.0
    elif name == "fast_current_pulses":
        passed = max_abs_error <= 8.0 and 60 <= internal_end <= 70
    elif name == "deep_cutoff":
        passed = min_vmin <= 3050 and internal_end == 0 and public_end <= 5
    elif name == "charge_anchor":
        bulk_rows = [row for row in rows if row["segment"] == "bulk-charge"]
        bulk_max = max(int(row["internal_soc"]) for row in bulk_rows) if bulk_rows else internal_end
        passed = bulk_max <= 99 and internal_end == 100
    else:
        passed = True

    return ScenarioSummary(
        name=name,
        duration_s=int(last["time_s"]),
        true_start=float(first["true_soc"]),
        true_end=float(last["true_soc"]),
        internal_start=int(first["internal_soc"]),
        internal_end=internal_end,
        public_end=public_end,
        max_abs_error=round(max_abs_error, 2),
        min_vmin=min_vmin,
        max_current_a10=max_current,
        passed=passed,
        note=scenario_note(name),
    )


def scale(value: float, src_min: float, src_max: float, dst_min: float, dst_max: float) -> float:
    if src_max <= src_min:
        return (dst_min + dst_max) / 2.0
    ratio = (value - src_min) / (src_max - src_min)
    return dst_min + ratio * (dst_max - dst_min)


def polyline(rows: list[dict[str, object]], key: str, x_min: float, x_max: float,
             y_min: float, y_max: float, left: float, top: float,
             width: float, height: float) -> str:
    points = []
    for row in rows:
        x = scale(float(row["time_s"]), x_min, x_max, left, left + width)
        y = scale(float(row[key]), y_min, y_max, top + height, top)
        points.append("{0:.1f},{1:.1f}".format(x, y))
    return " ".join(points)


def soc_chart(rows: list[dict[str, object]]) -> str:
    x_min = float(rows[0]["time_s"])
    x_max = float(rows[-1]["time_s"])
    left, top, width, height = 58.0, 20.0, 840.0, 220.0
    grid = []
    for value in (0, 25, 50, 75, 100):
        y = scale(value, 0, 100, top + height, top)
        grid.append(
            '<line x1="{0}" y1="{1:.1f}" x2="{2}" y2="{1:.1f}" class="grid" />'
            '<text x="18" y="{3:.1f}" class="axis">{4}%</text>'.format(
                left, y, left + width, y + 4.0, value
            )
        )
    true_points = polyline(rows, "true_soc", x_min, x_max, 0, 100, left, top, width, height)
    internal_points = polyline(rows, "internal_soc", x_min, x_max, 0, 100, left, top, width, height)
    public_points = polyline(rows, "public_soc", x_min, x_max, 0, 100, left, top, width, height)
    return """
<svg viewBox="0 0 930 280" role="img" aria-label="SOC curve">
  <rect x="0" y="0" width="930" height="280" class="plot-bg" />
  {grid}
  <line x1="{left}" y1="{bottom}" x2="{right}" y2="{bottom}" class="axis-line" />
  <line x1="{left}" y1="{top}" x2="{left}" y2="{bottom}" class="axis-line" />
  <polyline points="{true_points}" class="line true" />
  <polyline points="{internal_points}" class="line internal" />
  <polyline points="{public_points}" class="line public" />
  <text x="{left}" y="264" class="axis">0s</text>
  <text x="{right}" y="264" text-anchor="end" class="axis">{duration}s</text>
</svg>
""".format(
        grid="\n  ".join(grid),
        left=left,
        right=left + width,
        top=top,
        bottom=top + height,
        true_points=true_points,
        internal_points=internal_points,
        public_points=public_points,
        duration=int(rows[-1]["time_s"]),
    )


def voltage_current_chart(rows: list[dict[str, object]]) -> str:
    x_min = float(rows[0]["time_s"])
    x_max = float(rows[-1]["time_s"])
    v_values = [int(row["vmin_mv"]) for row in rows]
    c_values = [max(int(row["ichg_a10"]), int(row["idsg_a10"])) for row in rows]
    v_min = max(2400, min(v_values) - 80)
    v_max = min(4300, max(v_values) + 80)
    c_max = max(10, max(c_values))
    left, top, width, height = 58.0, 10.0, 840.0, 145.0
    v_points = polyline(rows, "vmin_mv", x_min, x_max, v_min, v_max, left, top, width, height)
    current_points = []
    for row in rows:
        current = max(int(row["ichg_a10"]), int(row["idsg_a10"]))
        x = scale(float(row["time_s"]), x_min, x_max, left, left + width)
        y = scale(current, 0, c_max, top + height, top)
        current_points.append("{0:.1f},{1:.1f}".format(x, y))
    return """
<svg viewBox="0 0 930 190" role="img" aria-label="Voltage and current curve">
  <rect x="0" y="0" width="930" height="190" class="plot-bg" />
  <line x1="{left}" y1="{bottom}" x2="{right}" y2="{bottom}" class="axis-line" />
  <line x1="{left}" y1="{top}" x2="{left}" y2="{bottom}" class="axis-line" />
  <text x="14" y="{top_text}" class="axis">{vmax}mV</text>
  <text x="14" y="{bottom_text}" class="axis">{vmin}mV</text>
  <text x="{right}" y="{top_text}" text-anchor="end" class="axis">{cmax} A*10</text>
  <polyline points="{v_points}" class="line voltage" />
  <polyline points="{current_points}" class="line current" />
</svg>
""".format(
        left=left,
        right=left + width,
        top=top,
        bottom=top + height,
        top_text=top + 10.0,
        bottom_text=top + height - 4.0,
        vmax=v_max,
        vmin=v_min,
        cmax=c_max,
        v_points=v_points,
        current_points=" ".join(current_points),
    )


def status_badge(passed: bool) -> str:
    return '<span class="badge {0}">{1}</span>'.format(
        "pass" if passed else "fail", "PASS" if passed else "FAIL"
    )


def render_html(grouped: dict[str, list[dict[str, object]]],
                summaries: list[ScenarioSummary],
                csv_path: Path,
                html_path: Path) -> None:
    summary_rows = []
    for item in summaries:
        summary_rows.append(
            "<tr><td>{name}</td><td>{status}</td><td>{duration}</td>"
            "<td>{true_start:.1f}->{true_end:.2f}</td>"
            "<td>{internal_start}->{internal_end}</td>"
            "<td>{public}</td><td>{err:.2f}</td><td>{vmin}</td><td>{current}</td>"
            "<td>{note}</td></tr>".format(
                name=html.escape(item.name),
                status=status_badge(item.passed),
                duration=item.duration_s,
                true_start=item.true_start,
                true_end=item.true_end,
                internal_start=item.internal_start,
                internal_end=item.internal_end,
                public=item.public_end,
                err=item.max_abs_error,
                vmin=item.min_vmin,
                current=item.max_current_a10,
                note=html.escape(item.note),
            )
        )

    sections = []
    for summary in summaries:
        rows = grouped[summary.name]
        sections.append(
            """
<section class="scenario">
  <div class="scenario-head">
    <div>
      <h2>{name}</h2>
      <p>{note}</p>
    </div>
    {status}
  </div>
  <div class="legend">
    <span><i class="dot true"></i>真实估算 SOC</span>
    <span><i class="dot internal"></i>真实 C 内部 SOC</span>
    <span><i class="dot public"></i>对外发布 SOC</span>
    <span><i class="dot voltage"></i>VCellMin</span>
    <span><i class="dot current"></i>电流</span>
  </div>
  {soc_chart}
  {vc_chart}
</section>
""".format(
                name=html.escape(summary.name),
                note=html.escape(summary.note),
                status=status_badge(summary.passed),
                soc_chart=soc_chart(rows),
                vc_chart=voltage_current_chart(rows),
            )
        )

    content = """<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>SOC 真实 C 逻辑可视化测试报告</title>
  <style>
    :root {{
      --bg: #f6f7f9;
      --panel: #ffffff;
      --text: #17202a;
      --muted: #64748b;
      --border: #d9e0ea;
      --true: #2563eb;
      --internal: #d97706;
      --public: #059669;
      --voltage: #475569;
      --current: #dc2626;
    }}
    * {{ box-sizing: border-box; }}
    body {{
      margin: 0;
      background: var(--bg);
      color: var(--text);
      font: 14px/1.55 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }}
    main {{
      max-width: 1160px;
      margin: 0 auto;
      padding: 28px 20px 48px;
    }}
    h1 {{ margin: 0 0 8px; font-size: 28px; }}
    h2 {{ margin: 0; font-size: 20px; }}
    p {{ margin: 0; color: var(--muted); }}
    .intro, .scenario, .summary {{
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 18px;
      margin-top: 16px;
    }}
    .intro ul {{ margin: 10px 0 0; padding-left: 20px; }}
    table {{ width: 100%; border-collapse: collapse; margin-top: 12px; }}
    th, td {{ border-bottom: 1px solid var(--border); padding: 8px 7px; text-align: left; vertical-align: top; }}
    th {{ color: var(--muted); font-weight: 600; }}
    .badge {{ display: inline-block; min-width: 54px; padding: 2px 8px; border-radius: 999px; text-align: center; font-weight: 700; font-size: 12px; }}
    .badge.pass {{ background: #dcfce7; color: #166534; }}
    .badge.fail {{ background: #fee2e2; color: #991b1b; }}
    .scenario-head {{ display: flex; justify-content: space-between; gap: 16px; align-items: flex-start; }}
    .legend {{ display: flex; flex-wrap: wrap; gap: 14px; margin: 14px 0 4px; color: var(--muted); }}
    .dot {{ display: inline-block; width: 10px; height: 10px; border-radius: 50%; margin-right: 6px; }}
    .dot.true {{ background: var(--true); }}
    .dot.internal {{ background: var(--internal); }}
    .dot.public {{ background: var(--public); }}
    .dot.voltage {{ background: var(--voltage); }}
    .dot.current {{ background: var(--current); }}
    svg {{ display: block; width: 100%; margin-top: 8px; }}
    .plot-bg {{ fill: #fbfcfe; }}
    .grid {{ stroke: #e5eaf1; stroke-width: 1; }}
    .axis-line {{ stroke: #aeb8c6; stroke-width: 1; }}
    .axis {{ fill: #64748b; font-size: 12px; }}
    .line {{ fill: none; stroke-width: 2.4; stroke-linecap: round; stroke-linejoin: round; }}
    .line.true {{ stroke: var(--true); }}
    .line.internal {{ stroke: var(--internal); }}
    .line.public {{ stroke: var(--public); }}
    .line.voltage {{ stroke: var(--voltage); }}
    .line.current {{ stroke: var(--current); opacity: 0.75; }}
    code {{ background: #eef2f7; padding: 2px 5px; border-radius: 4px; }}
  </style>
</head>
<body>
<main>
  <h1>SOC 真实 C 逻辑可视化测试报告</h1>
  <p>数据来源：编译并运行当前工程的 <code>SOC.c</code> + <code>SocEnhance.c</code>，CSV 明细为 <code>{csv_name}</code>。</p>

  <section class="intro">
    <h2>怎么看</h2>
    <ul>
      <li>蓝线是真实容量推算 SOC，用来给测试场景提供参考基准。</li>
      <li>橙线是真实 C 代码内部 SOC，来自 host trace harness 的 Flash snapshot。</li>
      <li>绿线是对外发布 SOC；当前已取消独立显示平滑层，因此它等于内部 SOC。</li>
      <li>灰线是最低单体电压，红线是电流，用来判断压降和校准是否合理。</li>
    </ul>
  </section>

  <section class="summary">
    <h2>汇总</h2>
    <table>
      <thead>
        <tr>
          <th>场景</th><th>结果</th><th>时长(s)</th><th>真实 SOC</th><th>内部 SOC</th>
          <th>发布 SOC</th><th>最大误差</th><th>最低 Vmin</th><th>最大电流(A*10)</th><th>判断点</th>
        </tr>
      </thead>
      <tbody>
        {summary_rows}
      </tbody>
    </table>
  </section>

  {sections}
</main>
</body>
</html>
""".format(
        csv_name=html.escape(csv_path.name),
        summary_rows="\n        ".join(summary_rows),
        sections="\n".join(sections),
    )
    html_path.write_text(content, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate a visual SOC report from real C host trace.")
    parser.add_argument("--html", default=str(BUILD_DIR / "soc_visual_report.html"))
    parser.add_argument("--csv", default=str(BUILD_DIR / "soc_visual_trace.csv"))
    args = parser.parse_args()

    html_path = Path(args.html)
    csv_path = Path(args.csv)
    html_path.parent.mkdir(parents=True, exist_ok=True)
    csv_path.parent.mkdir(parents=True, exist_ok=True)

    exe = BUILD_DIR / "soc_host_visual_trace"
    build_trace_exe(compiler(), exe)
    run_trace(exe, csv_path)
    rows = load_rows(csv_path)
    grouped = group_rows(rows)
    summaries = [summarize(name, grouped[name]) for name in grouped]
    render_html(grouped, summaries, csv_path, html_path)
    for item in summaries:
        print("{0}: {1}".format(item.name, "PASS" if item.passed else "FAIL"))
    print("html={0}".format(html_path))
    return 0 if all(item.passed for item in summaries) else 1


if __name__ == "__main__":
    raise SystemExit(main())
