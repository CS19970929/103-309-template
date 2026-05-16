#!/usr/bin/env python3
"""Generate reusable SOC ride simulation reports.

The simulator feeds the host-side SocModel with voltage/current generated from
a simple pack model. It is intended for regression testing SOC behavior before
flashing boards.
"""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from soc_replay_test import (
    CAP_FACTORY_AS10,
    CAP_A10,
    FULL_SECONDS,
    PERIOD_MS,
    Snapshot,
    SocModel,
    TICKS_PER_SECOND,
    voltage_from_soc,
)


@dataclass(frozen=True)
class Segment:
    name: str
    seconds: int
    idsg_a10: int = 0
    ichg_a10: int = 0
    imbalance_mv: int = 4


@dataclass
class ScenarioResult:
    name: str
    duration_s: int
    true_start: float
    true_end: float
    soc_start: int
    soc_end: int
    display_end: int
    max_abs_error: float
    min_vmin: int
    max_current_a10: int
    passed: bool
    notes: str


class TruePack:
    def __init__(self, soc: float, capacity_as10: int = CAP_FACTORY_AS10) -> None:
        self.capacity_as10 = capacity_as10
        self.cap_now = capacity_as10 * soc / 100.0

    @property
    def soc(self) -> float:
        return max(0.0, min(100.0, self.cap_now * 100.0 / self.capacity_as10))

    def step(self, idsg_a10: int, ichg_a10: int) -> None:
        delta = (ichg_a10 - idsg_a10) * PERIOD_MS / 1000.0
        self.cap_now = max(0.0, min(float(self.capacity_as10), self.cap_now + delta))

    def voltage(self, idsg_a10: int, ichg_a10: int, imbalance_mv: int) -> tuple[int, int]:
        base = voltage_from_soc(int(round(self.soc)))
        if idsg_a10:
            sag_mv = min(560, 15 + idsg_a10 // 2)
            vmin = max(2400, base - sag_mv - imbalance_mv)
        elif ichg_a10:
            rise_mv = min(130, 10 + ichg_a10 // 8)
            vmin = min(4200, base + rise_mv)
        else:
            vmin = base
        return min(5000, vmin + imbalance_mv), vmin


def scenario_city_commute() -> list[Segment]:
    return [
        Segment("idle-before-ride", 60),
        Segment("flat-cruise", 300, idsg_a10=80),
        Segment("traffic-bursts", 120, idsg_a10=220, imbalance_mv=6),
        Segment("steady-cruise", 300, idsg_a10=120),
        Segment("traffic-stop", 60),
        Segment("short-hill", 180, idsg_a10=350, imbalance_mv=10),
        Segment("return-cruise", 300, idsg_a10=100),
    ]


def scenario_hill_climb() -> list[Segment]:
    return [
        Segment("approach", 120, idsg_a10=120),
        Segment("long-hill", 240, idsg_a10=420, imbalance_mv=12),
        Segment("coast-recovery", 180),
        Segment("post-hill-cruise", 180, idsg_a10=100),
    ]


def scenario_fast_current_pulses() -> list[Segment]:
    pattern = [
        Segment("pulse-light", 1, idsg_a10=30, imbalance_mv=4),
        Segment("pulse-accelerate", 1, idsg_a10=260, imbalance_mv=8),
        Segment("pulse-cruise", 1, idsg_a10=80, imbalance_mv=4),
        Segment("pulse-steep", 1, idsg_a10=420, imbalance_mv=12),
        Segment("pulse-coast", 1, idsg_a10=0, imbalance_mv=4),
        Segment("pulse-recover", 1, idsg_a10=160, imbalance_mv=6),
        Segment("pulse-restart", 1, idsg_a10=320, imbalance_mv=10),
        Segment("pulse-roll", 1, idsg_a10=40, imbalance_mv=4),
    ]
    return pattern * 45


def scenario_deep_cutoff() -> list[Segment]:
    return [
        Segment("low-soc-cruise", 240, idsg_a10=120, imbalance_mv=6),
        Segment("low-soc-load", 420, idsg_a10=180, imbalance_mv=8),
        Segment("controller-cutoff-area", 240, idsg_a10=220, imbalance_mv=10),
    ]


def scenario_charge_anchor() -> list[Segment]:
    return [
        Segment("bulk-charge", 600, ichg_a10=270),
        Segment("near-full-confirm", FULL_SECONDS + 5, ichg_a10=270),
    ]


SCENARIOS = {
    "city_commute": (80.0, scenario_city_commute),
    "hill_climb": (60.0, scenario_hill_climb),
    "fast_current_pulses": (70.0, scenario_fast_current_pulses),
    "deep_cutoff": (18.0, scenario_deep_cutoff),
    "charge_anchor": (88.0, scenario_charge_anchor),
}


def run_scenario(name: str, sample_rows: list[dict[str, object]]) -> ScenarioResult:
    start_soc, factory = SCENARIOS[name]
    pack = TruePack(start_soc)
    model = SocModel.from_snapshot(
        Snapshot(soc=int(start_soc), cap_now=CAP_FACTORY_AS10 * int(start_soc) // 100)
    )
    max_abs_error = 0.0
    min_vmin = 5000
    max_current = 0
    duration = 0
    soc_start = model.soc
    charge_pre_anchor_max = model.soc

    for segment in factory():
        for tick in range(segment.seconds * TICKS_PER_SECOND):
            if name == "charge_anchor" and segment.name == "bulk-charge":
                vmax, vmin = 4100, 4050
            elif name == "charge_anchor" and segment.name == "near-full-confirm":
                vmax, vmin = 4180, 4100
            else:
                vmax, vmin = pack.voltage(
                    segment.idsg_a10, segment.ichg_a10, segment.imbalance_mv
                )
            model.tick(vmax=vmax, vmin=vmin, ichg=segment.ichg_a10, idsg=segment.idsg_a10)
            pack.step(segment.idsg_a10, segment.ichg_a10)
            duration += PERIOD_MS
            min_vmin = min(min_vmin, vmin)
            max_current = max(max_current, segment.idsg_a10, segment.ichg_a10)
            error = abs(model.soc - pack.soc)
            max_abs_error = max(max_abs_error, error)
            if name == "charge_anchor" and segment.name == "bulk-charge":
                charge_pre_anchor_max = max(charge_pre_anchor_max, model.soc)
            if tick % TICKS_PER_SECOND == 0:
                sample_rows.append(
                    {
                        "scenario": name,
                        "time_s": duration // 1000,
                        "segment": segment.name,
                        "true_soc": round(pack.soc, 2),
                        "internal_soc": model.soc,
                        "display_soc": model.display_soc,
                        "vmin_mv": vmin,
                        "vmax_mv": vmax,
                        "ichg_a10": segment.ichg_a10,
                        "idsg_a10": segment.idsg_a10,
                        "soh": model.soh,
                        "cap_now_ah100": (model.cap_now + 180) // 360,
                    }
                )

    notes = ""
    passed = True
    if name == "city_commute":
        passed = max_abs_error <= 8.0 and 50 <= model.soc <= 75
        notes = "城市混合骑行应平滑跟随安时积分"
    elif name == "hill_climb":
        passed = model.soc > 40 and max_abs_error <= 12.0
        notes = "大电流压降不能误判空电"
    elif name == "fast_current_pulses":
        passed = max_abs_error <= 8.0 and 60 <= model.soc <= 70
        notes = "200ms/5Hz current pulse tracking should follow sampled average current"
    elif name == "deep_cutoff":
        passed = min_vmin <= 3050 and model.soc == 0 and model.display_soc <= 5
        notes = "接近控制器截止电压时应收敛到零"
    elif name == "charge_anchor":
        passed = charge_pre_anchor_max <= 99 and model.soc == 100
        notes = "满电电压锚点确认前不应发布未确认 100"

    return ScenarioResult(
        name=name,
        duration_s=duration // 1000,
        true_start=start_soc,
        true_end=round(pack.soc, 2),
        soc_start=soc_start,
        soc_end=model.soc,
        display_end=model.display_soc,
        max_abs_error=round(max_abs_error, 2),
        min_vmin=min_vmin,
        max_current_a10=max_current,
        passed=passed,
        notes=notes,
    )


def write_csv(path: Path, rows: Iterable[dict[str, object]]) -> None:
    rows = list(rows)
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def write_report(path: Path, results: list[ScenarioResult], csv_path: Path) -> None:
    lines = [
        "# SOC 真实骑行模拟测试报告",
        "",
        "## 测试模型",
        "",
        "- 电流输入使用 `A * 10`，与固件 `u16Ichg/u16IDischg` 一致。",
        "- MCU SOC 安时积分节拍为 `200ms/5Hz`；快变电流测试按每个 200ms 样本平均电流校验。",
        "- 电压由真实容量 SOC 反推 OCV，并叠加放电压降、充电极化和单体不一致。",
        "- 被测对象是 `tools/soc_replay_test.py` 中镜像 `SocEnhance.c` 的主机模型。",
        "- CSV 明细文件：`{0}`。".format(csv_path.name),
        "",
        "## 汇总",
        "",
        "| 工况 | 时长(s) | 真实SOC 起止 | 算法SOC 起止 | 显示SOC | 最大误差 | 最低Vmin | 最大电流(A*10) | 结果 | 说明 |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---|---|",
    ]
    for r in results:
        lines.append(
            "| {name} | {duration_s} | {true_start:.1f}->{true_end:.2f} | "
            "{soc_start}->{soc_end} | {display_end} | {max_abs_error:.2f} | "
            "{min_vmin} | {max_current_a10} | {status} | {notes} |".format(
                name=r.name,
                duration_s=r.duration_s,
                true_start=r.true_start,
                true_end=r.true_end,
                soc_start=r.soc_start,
                soc_end=r.soc_end,
                display_end=r.display_end,
                max_abs_error=r.max_abs_error,
                min_vmin=r.min_vmin,
                max_current_a10=r.max_current_a10,
                status="PASS" if r.passed else "FAIL",
                notes=r.notes,
            )
        )
    lines.extend(
        [
            "",
            "## 复用方式",
            "",
            "```powershell",
            "py tools\\soc_ride_sim_report.py --report SOC_RIDE_SIM_REPORT.md --csv SOC_RIDE_SIM_SAMPLES.csv",
            "```",
            "",
            "新增项目复用时，优先调整 `SCENARIOS` 中的容量、起始 SOC、工况段电流和压降参数，再用该报告对比固件变更前后的 SOC 轨迹。",
        ]
    )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate SOC ride simulation report.")
    parser.add_argument("--report", default="SOC_RIDE_SIM_REPORT.md")
    parser.add_argument("--csv", default="SOC_RIDE_SIM_SAMPLES.csv")
    args = parser.parse_args()

    rows: list[dict[str, object]] = []
    results = [run_scenario(name, rows) for name in SCENARIOS]
    csv_path = Path(args.csv)
    report_path = Path(args.report)
    write_csv(csv_path, rows)
    write_report(report_path, results, csv_path)
    for result in results:
        print("{0}: {1}".format(result.name, "PASS" if result.passed else "FAIL"))
    print("report={0}".format(report_path))
    print("csv={0}".format(csv_path))
    return 0 if all(result.passed for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
