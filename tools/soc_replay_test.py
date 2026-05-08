#!/usr/bin/env python3
"""Host-side replay tests for the simplified e-bike SOC model.

The model mirrors the rewritten SocEnhance.c decisions that matter for storage,
anchoring, SOH mapping, and display smoothing. It intentionally avoids STM32
bindings so it can run in CI or during local review.
"""

from dataclasses import dataclass


TICKS_PER_SECOND = 5
PERIOD_MS = 200
CURRENT_ENTER_A10 = 4
DEFAULT_SOC = 60
CAP_A10 = 270
CAP_FACTORY_AS10 = CAP_A10 * 3600
SOH_MIN = 70
SOH_STEP_CYCLES = 50
FULL_TAPER_A10 = max(CURRENT_ENTER_A10, (CAP_A10 + 19) // 20)
FULL_SECONDS = 60
EMPTY_SECONDS = 4
REST_OCV_SECONDS = 1800
LOW_GUARD_SECONDS = 10
LOW_GUARD_CURRENT_A10 = max(CURRENT_ENTER_A10, (CAP_A10 + 4) // 5)

MODE_RELAX = 0
MODE_CHG = 1
MODE_DSG = 2

OCV_TABLE = [
    (4160, 100), (4100, 95), (4050, 90), (3995, 85), (3935, 80),
    (3880, 75), (3835, 70), (3795, 65), (3760, 60), (3725, 55),
    (3695, 50), (3670, 45), (3645, 40), (3615, 35), (3585, 30),
    (3555, 25), (3525, 20), (3480, 15), (3400, 10), (3250, 5),
    (3000, 0),
]


@dataclass
class Snapshot:
    valid: bool = True
    soc: int = DEFAULT_SOC
    cap_now: int = 0
    cap_full: int = 0
    cycle_x100: int = 300
    cycle_acc_as10: int = 0
    vmax: int = 0
    vmin: int = 0


def interp_soc(mv):
    for (x1, y1), (x2, y2) in zip(OCV_TABLE, OCV_TABLE[1:]):
        if x1 >= mv >= x2:
            if x1 == x2:
                return y1
            return int((y1 * (mv - x2) + y2 * (x1 - mv)) / (x1 - x2))
    return OCV_TABLE[0][1] if mv >= OCV_TABLE[0][0] else OCV_TABLE[-1][1]


def step_toward(current, target, step):
    if current < target:
        return min(target, current + step)
    if current > target:
        return max(target, current - step)
    return current


def soh_from_cycle(cycle_x100):
    drop = (cycle_x100 // 100) // SOH_STEP_CYCLES
    return max(SOH_MIN, 100 - drop)


@dataclass
class SocModel:
    soc: int = DEFAULT_SOC
    display_soc: int = DEFAULT_SOC
    cycle_x100: int = 300
    cycle_acc_as10: int = 0
    cap_factory: int = CAP_FACTORY_AS10
    cap_full: int = CAP_FACTORY_AS10
    cap_now: int = 0
    soh: int = 100
    mode: int = MODE_RELAX
    last_mode: int = MODE_RELAX
    remainder_ms: int = 0
    full_ticks: int = 0
    empty_ticks: int = 0
    low_guard_ticks: int = 0
    display_ticks: int = 0
    full_anchor: bool = False
    fixed: bool = False
    zero: bool = False

    @classmethod
    def from_snapshot(cls, snapshot):
        model = cls()
        if not snapshot.valid:
            model.cycle_x100 = 300
            model.recalc_full()
            if model.voltage_allowed(snapshot.vmax, snapshot.vmin):
                model.set_soc(interp_soc(snapshot.vmin))
            else:
                model.set_soc(DEFAULT_SOC)
            model.display_soc = model.soc
            return model
        model.cycle_x100 = snapshot.cycle_x100
        model.recalc_full()
        model.cycle_acc_as10 = snapshot.cycle_acc_as10 % (model.cap_factory // 100)
        if snapshot.cap_now or snapshot.soc == 0:
            model.cap_now = min(snapshot.cap_now, model.cap_full)
            model.soc = model.soc_from_cap()
        else:
            model.set_soc(snapshot.soc)
        model.display_soc = model.soc
        model.full_anchor = model.soc >= 100
        return model

    def recalc_full(self):
        self.soh = soh_from_cycle(self.cycle_x100)
        self.cap_full = self.cap_factory * self.soh // 100
        self.cap_now = min(self.cap_now, self.cap_full)

    def soc_from_cap(self):
        if self.cap_now >= self.cap_full:
            return 100
        return int((self.cap_now * 100 + self.cap_full // 2) // self.cap_full)

    def set_soc(self, soc):
        self.soc = max(0, min(100, soc))
        self.cap_now = self.soc * self.cap_full // 100
        self.remainder_ms = 0
        self.full_anchor = self.soc >= 100

    def direction(self, ichg, idsg):
        if ichg >= CURRENT_ENTER_A10 and ichg >= idsg:
            return MODE_CHG
        if idsg >= CURRENT_ENTER_A10:
            return MODE_DSG
        return MODE_RELAX

    def voltage_allowed(self, vmax, vmin, fault=False):
        return not fault and 2000 <= vmin <= vmax <= 5000 and (vmax - vmin) <= 1000

    def add_cycle_capacity(self, delta):
        unit = self.cap_factory // 100
        self.cycle_acc_as10 += delta
        while self.cycle_acc_as10 >= unit:
            self.cycle_acc_as10 -= unit
            self.cycle_x100 += 1
        old_soh = self.soh
        self.recalc_full()
        if self.soh != old_soh:
            self.soc = self.soc_from_cap()

    def integrate(self, direction, current):
        if direction != self.last_mode:
            self.remainder_ms = 0
            self.last_mode = direction
        if direction == MODE_RELAX or current == 0:
            self.remainder_ms = 0
            return
        acc_ms = current * PERIOD_MS + self.remainder_ms
        delta = acc_ms // 1000
        self.remainder_ms = acc_ms % 1000
        if delta == 0:
            return
        if direction == MODE_CHG:
            self.cap_now = min(self.cap_full, self.cap_now + delta)
        else:
            self.full_anchor = False
            self.add_cycle_capacity(delta)
            self.cap_now = max(0, self.cap_now - delta)
        self.soc = self.soc_from_cap()
        if direction == MODE_CHG and not self.full_anchor and self.soc >= 100:
            self.soc = 99
            self.cap_now = self.cap_full * 99 // 100

    def apply_full_empty(self, direction, vmax, vmin, ichg):
        if direction == MODE_CHG:
            self.empty_ticks = 0
            if self.voltage_allowed(vmax, vmin) and ichg <= FULL_TAPER_A10 and vmax >= 4180 and vmin >= 4120 and (vmax - vmin) <= 120:
                self.full_ticks += 1
                if self.full_ticks >= FULL_SECONDS * TICKS_PER_SECOND:
                    self.set_soc(100)
                    self.full_ticks = 0
                    self.display_soc = 100
                return
            self.full_ticks = 0
            return
        self.full_ticks = 0
        if direction != MODE_DSG or not self.voltage_allowed(vmax, vmin):
            self.empty_ticks = 0
            return
        if vmin <= 2500:
            self.set_soc(0)
            self.display_soc = 0
            self.empty_ticks = 0
        elif vmin <= 2750:
            if self.soc > 1:
                self.set_soc(1)
            self.display_soc = self.soc
            self.empty_ticks = 0
        elif vmin <= 3000:
            self.empty_ticks += 1
            if self.empty_ticks >= EMPTY_SECONDS * TICKS_PER_SECOND:
                self.set_soc(step_toward(self.soc, 0, 5))
                self.empty_ticks = 0

    def apply_low_voltage_guard(self, direction, vmax, vmin, idsg):
        if direction == MODE_CHG or not self.voltage_allowed(vmax, vmin) or vmin > 3400:
            self.low_guard_ticks = 0
            return
        if direction == MODE_DSG and idsg > LOW_GUARD_CURRENT_A10:
            self.low_guard_ticks = 0
            return
        target = interp_soc(vmin)
        margin = 3 if vmin <= 3250 else 8
        limit = min(100, target + margin)
        if self.soc > limit:
            self.low_guard_ticks += 1
            if self.low_guard_ticks >= LOW_GUARD_SECONDS * TICKS_PER_SECOND:
                self.set_soc(step_toward(self.soc, limit, 1))
                self.low_guard_ticks = 0
        else:
            self.low_guard_ticks = 0

    def apply_rest_ocv(self, rest_seconds, vmax, vmin, direction=MODE_RELAX, fault=False):
        if rest_seconds < REST_OCV_SECONDS or not self.voltage_allowed(vmax, vmin, fault):
            return False
        target = interp_soc(vmin)
        if direction == MODE_CHG and target <= self.soc:
            return False
        if direction == MODE_DSG and target >= self.soc:
            return False
        step = 3 if rest_seconds >= 21600 else 2 if rest_seconds >= 3600 else 1
        old = self.soc
        self.set_soc(step_toward(self.soc, target, step))
        return self.soc != old

    def display_target(self):
        if self.zero:
            return 0
        if self.fixed:
            return 60
        return self.soc

    def update_display(self, vmin=3600, force=False):
        target = self.display_target()
        if force or self.zero or self.fixed:
            self.display_soc = target
            self.display_ticks = 0
            return
        if self.display_soc == target:
            self.display_ticks = 0
            return
        seconds = 5
        if target < self.display_soc and vmin <= 3000:
            seconds = 1
        elif target > self.display_soc and self.mode == MODE_CHG:
            seconds = 10
        self.display_ticks += 1
        if self.display_ticks >= seconds * TICKS_PER_SECOND:
            self.display_soc += 1 if self.display_soc < target else -1
            self.display_ticks = 0

    def tick(self, vmax=3600, vmin=3600, ichg=0, idsg=0, fault=False):
        self.mode = self.direction(ichg, idsg)
        self.integrate(self.mode, ichg if self.mode == MODE_CHG else idsg)
        self.apply_full_empty(self.mode, vmax, vmin, ichg)
        self.apply_low_voltage_guard(self.mode, vmax, vmin, idsg)
        self.update_display(vmin=vmin)


def run_seconds(model, seconds, **kwargs):
    for _ in range(seconds * TICKS_PER_SECOND):
        model.tick(**kwargs)


def test_invalid_snapshot_defaults_to_60_percent():
    model = SocModel.from_snapshot(Snapshot(valid=False))
    assert model.soc == 60
    assert model.display_soc == 60
    assert model.soh == 100


def test_invalid_snapshot_uses_valid_startup_ocv():
    model = SocModel.from_snapshot(Snapshot(valid=False, vmax=3835, vmin=3835))
    assert model.soc == 70
    assert model.display_soc == 70


def test_valid_snapshot_restores_capacity_and_cycle_soh():
    cap_full = CAP_FACTORY_AS10 * 98 // 100
    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=cap_full * 80 // 100, cycle_x100=10000))
    assert model.soc == 80
    assert model.soh == 98
    assert model.cap_full == cap_full


def test_set_soc_once_syncs_internal_capacity_and_display():
    model = SocModel.from_snapshot(Snapshot(valid=False))
    model.set_soc(35)
    model.update_display(force=True)
    assert model.soc == 35
    assert model.display_soc == 35
    assert model.cap_now == model.cap_full * 35 // 100


def test_discharge_integration_reduces_soc_and_counts_cycle_fraction():
    model = SocModel.from_snapshot(Snapshot(valid=False))
    run_seconds(model, 360, idsg=270)
    assert 49 <= model.soc <= 51
    assert model.cycle_x100 % 100 == 10


def test_charge_integration_stops_display_before_full_confirm():
    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    run_seconds(model, 900, ichg=270, vmax=4100, vmin=4050)
    assert model.soc == 99
    assert model.display_soc < 100


def test_soh_maps_cycles_to_capacity_floor():
    assert soh_from_cycle(0) == 100
    assert soh_from_cycle(5000) == 99
    assert soh_from_cycle(150000) == 70
    assert soh_from_cycle(250000) == 70


def test_full_confirm_requires_taper_and_balanced_voltage():
    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    run_seconds(model, 60, ichg=FULL_TAPER_A10 + 1, vmax=4180, vmin=4120)
    assert model.soc != 100
    run_seconds(model, 60, ichg=FULL_TAPER_A10, vmax=4180, vmin=4120)
    assert model.soc == 100
    assert model.display_soc == 100


def test_empty_anchor_limits_low_voltage_tail():
    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    run_seconds(model, 4, idsg=40, vmax=3000, vmin=3000)
    assert model.soc == 25
    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    model.tick(idsg=40, vmax=2750, vmin=2750)
    assert model.soc == 1
    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    model.tick(idsg=40, vmax=2500, vmin=2500)
    assert model.soc == 0


def test_rtc_rest_ocv_applies_small_bounded_step():
    model = SocModel.from_snapshot(Snapshot(soc=50, cap_now=CAP_FACTORY_AS10 * 50 // 100))
    changed = model.apply_rest_ocv(21600, vmax=3835, vmin=3835)
    assert changed
    assert model.soc == 53
    model.update_display()
    assert model.display_soc == 50
    for _ in range(5 * TICKS_PER_SECOND - 1):
        model.update_display()
    assert model.display_soc == 51


def test_ocv_correction_blocks_fault_and_direction_errors():
    model = SocModel.from_snapshot(Snapshot(soc=50, cap_now=CAP_FACTORY_AS10 * 50 // 100))
    assert not model.apply_rest_ocv(21600, vmax=3835, vmin=3835, direction=MODE_DSG)
    assert model.soc == 50
    assert not model.apply_rest_ocv(21600, vmax=3835, vmin=3835, fault=True)
    assert model.soc == 50


def test_display_smoothing_charge_and_low_voltage_down():
    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=CAP_FACTORY_AS10 * 80 // 100))
    model.display_soc = 79
    model.mode = MODE_CHG
    for _ in range(10 * TICKS_PER_SECOND - 1):
        model.update_display()
    assert model.display_soc == 79
    model.update_display()
    assert model.display_soc == 80
    model.soc = 70
    model.display_soc = 75
    for _ in range(TICKS_PER_SECOND):
        model.update_display(vmin=3000)
    assert model.display_soc == 74


def test_fixed_and_zero_overlay_do_not_change_internal_soc():
    model = SocModel.from_snapshot(Snapshot(soc=72, cap_now=CAP_FACTORY_AS10 * 72 // 100))
    model.fixed = True
    model.update_display(force=True)
    assert model.display_soc == 60
    assert model.soc == 72
    model.fixed = False
    model.zero = True
    model.update_display(force=True)
    assert model.display_soc == 0
    assert model.soc == 72


def test_low_voltage_guard_is_light_load_only_and_rate_limited():
    model = SocModel.from_snapshot(Snapshot(soc=40, cap_now=CAP_FACTORY_AS10 * 40 // 100))
    run_seconds(model, LOW_GUARD_SECONDS - 1, idsg=20, vmax=3400, vmin=3400)
    assert model.soc == 40
    run_seconds(model, 1, idsg=20, vmax=3400, vmin=3400)
    assert model.soc == 39
    model = SocModel.from_snapshot(Snapshot(soc=40, cap_now=CAP_FACTORY_AS10 * 40 // 100))
    run_seconds(model, LOW_GUARD_SECONDS + 1, idsg=LOW_GUARD_CURRENT_A10 + 1, vmax=3400, vmin=3400)
    assert model.soc == 40


def main():
    tests = [
        test_invalid_snapshot_defaults_to_60_percent,
        test_invalid_snapshot_uses_valid_startup_ocv,
        test_valid_snapshot_restores_capacity_and_cycle_soh,
        test_set_soc_once_syncs_internal_capacity_and_display,
        test_discharge_integration_reduces_soc_and_counts_cycle_fraction,
        test_charge_integration_stops_display_before_full_confirm,
        test_soh_maps_cycles_to_capacity_floor,
        test_full_confirm_requires_taper_and_balanced_voltage,
        test_empty_anchor_limits_low_voltage_tail,
        test_rtc_rest_ocv_applies_small_bounded_step,
        test_ocv_correction_blocks_fault_and_direction_errors,
        test_display_smoothing_charge_and_low_voltage_down,
        test_fixed_and_zero_overlay_do_not_change_internal_soc,
        test_low_voltage_guard_is_light_load_only_and_rate_limited,
    ]
    for test in tests:
        test()
        print('PASS {0}'.format(test.__name__))
    print('SOC replay tests passed: {0}'.format(len(tests)))


if __name__ == '__main__':
    main()
