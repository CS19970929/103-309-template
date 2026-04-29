#!/usr/bin/env python3
"""Replay checks for SOC calibration edge cases.

This is a host-side strategy test. It mirrors the bounded calibration decisions
from SocEnhance.c without depending on STM32 or Keil libraries.
"""

from __future__ import print_function

from dataclasses import dataclass


SOC_TICKS_PER_SECOND = 5
SOC_CURRENT_ENTER_A10 = 4
SOC_FULL_CONFIRM_SECONDS = 60
SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV = 80
SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV = 120
SOC_ONLINE_OCV_CORRECTION_SECONDS = 30
SOC_ONLINE_OCV_MIN_DELTA_PERCENT = 3
SOC_ONLINE_OCV_CURRENT_DIVIDER = 10
SOC_ONLINE_OCV_HEAVY_DSG_CURRENT_DIVIDER = 3
SOC_ONLINE_OCV_HEAVY_DSG_HOLDOFF_SECONDS = 180
SOC_ONLINE_OCV_STABLE_SECONDS = 20
SOC_ONLINE_OCV_STABLE_WINDOW_MV = 8
SOC_CALIBRATION_MIN_CELL_VALID_MV = 2000
SOC_CALIBRATION_MAX_CELL_VALID_MV = 5000
SOC_CALIBRATION_MAX_CELL_DELTA_MV = 1000
SOC_ONLINE_OCV_TARGET_MIN_PERCENT = 5
SOC_ONLINE_OCV_TARGET_MAX_PERCENT = 95
SOC_ONLINE_OCV_LFP_LOW_EDGE_PERCENT = 20
SOC_ONLINE_OCV_LFP_HIGH_EDGE_PERCENT = 90

SOC_TABLE_TERNARYLI = 2
SOC_TABLE_LIFEPO = 1

TERNARY_TABLE = [
    (4126, 100),
    (4066, 95),
    (4011, 90),
    (3955, 85),
    (3888, 80),
    (3837, 75),
    (3793, 70),
    (3756, 65),
    (3724, 60),
    (3699, 55),
    (3675, 50),
    (3658, 45),
    (3632, 40),
    (3605, 35),
    (3584, 30),
    (3557, 25),
    (3535, 20),
    (3497, 15),
    (3475, 10),
    (3371, 5),
    (3136, 3),
]

LFP_TABLE = [
    (3336, 100),
    (3332, 90),
    (3330, 80),
    (3327, 75),
    (3316, 70),
    (3301, 65),
    (3294, 60),
    (3291, 55),
    (3290, 50),
    (3288, 45),
    (3286, 40),
    (3279, 35),
    (3266, 30),
    (3254, 25),
    (3236, 20),
    (3212, 15),
    (3198, 10),
    (3112, 5),
    (2526, 0),
    (1000, 0),
    (1000, 0),
]


def interp_soc(table, mv):
    for index in range(0, len(table) - 1):
        x1, y1 = table[index]
        x2, y2 = table[index + 1]
        if (x1 >= mv >= x2) or (x1 <= mv <= x2):
            if x1 == x2:
                return y1
            if x2 < x1:
                x1, x2 = x2, x1
                y1, y2 = y2, y1
            return int((y1 * (x2 - mv) + y2 * (mv - x1)) / (x2 - x1))
    return table[0][1] if mv >= table[0][0] else table[-1][1]


def step_toward(current, target, step):
    if current < target:
        return min(target, current + step)
    if current > target:
        return max(target, current - step)
    return current


@dataclass
class SocReplay:
    soc: int
    table_select: int = SOC_TABLE_TERNARYLI
    v100_mv: int = 4180
    capacity_a10: int = 270
    full_ticks: int = 0
    online_ticks: int = 0
    online_stable_ticks: int = 0
    online_recovery_holdoff_ticks: int = 0
    online_ref_vmin: int = 0
    online_ref_vmax: int = 0
    online_direction: int = 0
    protection_fault: bool = False
    system_fault: bool = False

    def table(self):
        return LFP_TABLE if self.table_select == SOC_TABLE_LIFEPO else TERNARY_TABLE

    def current_limit(self, divider):
        return max(SOC_CURRENT_ENTER_A10, int(self.capacity_a10 / max(divider, 1)))

    def full_confirm_min_cell_mv(self):
        chemistry_floor = 3300 if self.table_select == SOC_TABLE_LIFEPO else 4000
        return max(chemistry_floor, self.v100_mv - SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV)

    def full_delta_ok(self, vmax, vmin):
        return abs(vmax - vmin) <= SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV

    def voltage_valid(self, vmax, vmin):
        return (
            SOC_CALIBRATION_MIN_CELL_VALID_MV <= vmin <= SOC_CALIBRATION_MAX_CELL_VALID_MV
            and SOC_CALIBRATION_MIN_CELL_VALID_MV <= vmax <= SOC_CALIBRATION_MAX_CELL_VALID_MV
            and vmax >= vmin
        )

    def calibration_allowed(self, vmax, vmin):
        return (
            self.voltage_valid(vmax, vmin)
            and abs(vmax - vmin) <= SOC_CALIBRATION_MAX_CELL_DELTA_MV
            and not self.protection_fault
            and not self.system_fault
        )

    def target_trusted(self, target):
        if target < SOC_ONLINE_OCV_TARGET_MIN_PERCENT:
            return False
        if target > SOC_ONLINE_OCV_TARGET_MAX_PERCENT:
            return False
        if self.table_select == SOC_TABLE_LIFEPO:
            if SOC_ONLINE_OCV_LFP_LOW_EDGE_PERCENT < target < SOC_ONLINE_OCV_LFP_HIGH_EDGE_PERCENT:
                return False
        return True

    def reset_online_stability(self):
        self.online_stable_ticks = 0
        self.online_ref_vmin = 0
        self.online_ref_vmax = 0

    def reset_online_guard(self):
        self.online_ticks = 0
        self.online_direction = 0
        self.reset_online_stability()

    def track_online_recovery_holdoff(self, idsg):
        holdoff_limit = SOC_ONLINE_OCV_HEAVY_DSG_HOLDOFF_SECONDS * SOC_TICKS_PER_SECOND
        if holdoff_limit == 0:
            self.online_recovery_holdoff_ticks = 0
            return

        if idsg >= self.current_limit(SOC_ONLINE_OCV_HEAVY_DSG_CURRENT_DIVIDER):
            self.online_recovery_holdoff_ticks = holdoff_limit
            self.reset_online_guard()
            return

        if self.online_recovery_holdoff_ticks > 0:
            self.online_recovery_holdoff_ticks -= 1

    def update_online_voltage_stable(self, vmax, vmin):
        stable_limit = SOC_ONLINE_OCV_STABLE_SECONDS * SOC_TICKS_PER_SECOND
        if stable_limit == 0:
            return True

        if self.online_ref_vmin == 0 or self.online_ref_vmax == 0:
            self.online_ref_vmin = vmin
            self.online_ref_vmax = vmax
            self.online_stable_ticks = 0
            return False

        if (
            abs(vmin - self.online_ref_vmin) <= SOC_ONLINE_OCV_STABLE_WINDOW_MV
            and abs(vmax - self.online_ref_vmax) <= SOC_ONLINE_OCV_STABLE_WINDOW_MV
        ):
            if self.online_stable_ticks < stable_limit:
                self.online_stable_ticks += 1
            return self.online_stable_ticks >= stable_limit

        self.online_ref_vmin = vmin
        self.online_ref_vmax = vmax
        self.online_stable_ticks = 0
        return False

    def tick(self, vmax, vmin, ichg, idsg):
        direction = 0
        if ichg >= SOC_CURRENT_ENTER_A10 and ichg >= idsg:
            direction = 1
        elif idsg >= SOC_CURRENT_ENTER_A10:
            direction = 2

        self.track_online_recovery_holdoff(idsg)
        self.apply_full_confirm(direction, vmax, vmin, ichg)
        self.apply_online_ocv(direction, vmax, vmin, ichg, idsg)

    def apply_full_confirm(self, direction, vmax, vmin, ichg):
        if direction != 1 or not self.calibration_allowed(vmax, vmin):
            self.full_ticks = 0
            return
        if (
            vmax >= self.v100_mv
            and vmin >= self.full_confirm_min_cell_mv()
            and self.full_delta_ok(vmax, vmin)
            and ichg != 0
            and ichg <= self.current_limit(20)
        ):
            self.full_ticks += 1
            if self.full_ticks >= SOC_FULL_CONFIRM_SECONDS * SOC_TICKS_PER_SECOND:
                self.soc = 100
                self.full_ticks = 0
            return
        self.full_ticks = 0

    def apply_online_ocv(self, direction, vmax, vmin, ichg, idsg):
        if direction not in (1, 2):
            self.reset_online_guard()
            return

        if not self.calibration_allowed(vmax, vmin):
            self.reset_online_guard()
            return

        current = ichg if direction == 1 else idsg
        if not (SOC_CURRENT_ENTER_A10 <= current <= self.current_limit(SOC_ONLINE_OCV_CURRENT_DIVIDER)):
            self.reset_online_guard()
            return

        if self.online_recovery_holdoff_ticks != 0:
            self.reset_online_guard()
            return

        target = interp_soc(self.table(), vmin)
        if not self.target_trusted(target):
            self.reset_online_guard()
            return

        if direction == 1:
            target = min(target, SOC_ONLINE_OCV_TARGET_MAX_PERCENT)
            if target <= self.soc + SOC_ONLINE_OCV_MIN_DELTA_PERCENT:
                self.reset_online_guard()
                return
        else:
            target = max(target, SOC_ONLINE_OCV_TARGET_MIN_PERCENT)
            if self.soc <= target + SOC_ONLINE_OCV_MIN_DELTA_PERCENT:
                self.reset_online_guard()
                return

        if self.online_direction != direction:
            self.online_direction = direction
            self.online_ticks = 0
            self.reset_online_stability()

        if not self.update_online_voltage_stable(vmax, vmin):
            self.online_ticks = 0
            return

        self.online_ticks += 1
        if self.online_ticks >= SOC_ONLINE_OCV_CORRECTION_SECONDS * SOC_TICKS_PER_SECOND:
            self.online_ticks = 0
            self.soc = step_toward(self.soc, target, 1)


def run_seconds(model, seconds, vmax, vmin, ichg, idsg):
    for _ in range(seconds * SOC_TICKS_PER_SECOND):
        model.tick(vmax, vmin, ichg, idsg)


def test_full_confirm_rejects_imbalanced_pack():
    model = SocReplay(soc=98)
    run_seconds(model, 90, vmax=4180, vmin=4000, ichg=10, idsg=0)
    assert model.soc == 98


def test_full_confirm_accepts_balanced_taper_pack():
    model = SocReplay(soc=98)
    run_seconds(model, 60, vmax=4180, vmin=4110, ichg=10, idsg=0)
    assert model.soc == 100


def test_full_confirm_rejects_abnormal_voltage_sample():
    model = SocReplay(soc=98)
    run_seconds(model, 90, vmax=5100, vmin=4110, ichg=10, idsg=0)
    assert model.soc == 98


def test_online_ocv_discharge_converges_down_under_light_load():
    model = SocReplay(soc=80)
    run_seconds(model, 320, vmax=3756, vmin=3756, ichg=0, idsg=20)
    assert model.soc == 70


def test_online_ocv_charge_converges_up_under_light_load():
    model = SocReplay(soc=50)
    run_seconds(model, 320, vmax=3793, vmin=3793, ichg=20, idsg=0)
    assert model.soc == 60


def test_online_ocv_ignores_heavy_discharge_load():
    model = SocReplay(soc=80)
    run_seconds(model, 300, vmax=3756, vmin=3756, ichg=0, idsg=80)
    assert model.soc == 80


def test_online_ocv_holdoff_after_heavy_discharge():
    model = SocReplay(soc=80)
    run_seconds(model, 10, vmax=3700, vmin=3700, ichg=0, idsg=100)
    run_seconds(model, 180, vmax=3756, vmin=3756, ichg=0, idsg=20)
    assert model.soc == 80


def test_online_ocv_waits_for_voltage_rebound_to_stabilize():
    model = SocReplay(soc=80)
    for second in range(180):
        vmin = 3720 + second
        run_seconds(model, 1, vmax=vmin, vmin=vmin, ichg=0, idsg=20)
    assert model.soc == 80


def test_online_ocv_ignores_reversed_cell_range():
    model = SocReplay(soc=80)
    run_seconds(model, 300, vmax=3700, vmin=3756, ichg=0, idsg=20)
    assert model.soc == 80


def test_online_ocv_ignores_protection_fault():
    model = SocReplay(soc=80, protection_fault=True)
    run_seconds(model, 300, vmax=3756, vmin=3756, ichg=0, idsg=20)
    assert model.soc == 80


def test_online_ocv_ignores_system_fault():
    model = SocReplay(soc=50, system_fault=True)
    run_seconds(model, 300, vmax=3793, vmin=3793, ichg=20, idsg=0)
    assert model.soc == 50


def test_online_ocv_ignores_lfp_mid_plateau():
    model = SocReplay(soc=80, table_select=SOC_TABLE_LIFEPO, v100_mv=3600)
    run_seconds(model, 300, vmax=3294, vmin=3294, ichg=0, idsg=20)
    assert model.soc == 80


def main():
    tests = [
        test_full_confirm_rejects_imbalanced_pack,
        test_full_confirm_accepts_balanced_taper_pack,
        test_full_confirm_rejects_abnormal_voltage_sample,
        test_online_ocv_discharge_converges_down_under_light_load,
        test_online_ocv_charge_converges_up_under_light_load,
        test_online_ocv_ignores_heavy_discharge_load,
        test_online_ocv_holdoff_after_heavy_discharge,
        test_online_ocv_waits_for_voltage_rebound_to_stabilize,
        test_online_ocv_ignores_reversed_cell_range,
        test_online_ocv_ignores_protection_fault,
        test_online_ocv_ignores_system_fault,
        test_online_ocv_ignores_lfp_mid_plateau,
    ]
    for test in tests:
        test()
        print("PASS {0}".format(test.__name__))
    print("SOC replay tests passed: {0}".format(len(tests)))


if __name__ == "__main__":
    main()
