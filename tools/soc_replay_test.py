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
SOH_MIN = 80
SOH_STEP_CYCLES = 100
FULL_SECONDS = 15
FULL_FAST_SECONDS = 5
FULL_MIN_SOC = 95
FULL_CONFIRM_MARGIN_MV = 80
FULL_CONFIRM_MAX_DELTA_MV = 120
EMPTY_MV = 3000
REST_OCV_SECONDS = 1800
EMPTY_LIGHT_CURRENT_A10 = max(CURRENT_ENTER_A10, (CAP_A10 + 4) // 5)
EMPTY_MID_CURRENT_A10 = max(CURRENT_ENTER_A10, (CAP_A10 + 1) // 2)
CAL_STEP = 1
DISPLAY_NORMAL_SECONDS = 5
DISPLAY_CHG_SECONDS = DISPLAY_NORMAL_SECONDS
DISPLAY_LOW_SECONDS = 1
SAG_HOLDOFF_SECONDS = 30
SAG_ALLOW_MV = EMPTY_MV + 50
BLOCK_CALIBRATION_PROTECTION_FAULT = False
BLOCK_CALIBRATION_SYSTEM_FAULT = False

EMPTY_BAND_RELAX = 0
EMPTY_BAND_LIGHT = 1
EMPTY_BAND_MID = 2
EMPTY_BAND_HEAVY = 3

EMPTY_TAIL_TABLE = [
    (-50, (0, 0, 0, 0), (1, 1, 1, 1)),
    (-25, (0, 0, 0, 0), (5, 5, 1, 1)),
    (0, (0, 0, 0, 0), (10, 5, 5, 5)),
    (50, (4, 5, 8, 12), (20, 15, 10, 8)),
    (100, (8, 10, 14, 18), (35, 30, 25, 20)),
    (200, (12, 14, 20, 25), (60, 50, 40, 30)),
    (300, (14, 18, 25, 32), (90, 75, 60, 45)),
    (400, (18, 22, 30, 40), (120, 100, 80, 60)),
]

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
    rest_ticks: int = 0
    display_ticks: int = 0
    sag_hold_ticks: int = 0
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

    def voltage_allowed(self, vmax, vmin, fault=False, system_fault=False):
        if BLOCK_CALIBRATION_PROTECTION_FAULT and fault:
            return False
        if BLOCK_CALIBRATION_SYSTEM_FAULT and system_fault:
            return False
        return 2000 <= vmin <= vmax <= 5000 and (vmax - vmin) <= 1000

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

    def apply_full_empty(self, direction, vmax, vmin, ichg, idsg):
        old = self.soc
        if direction != MODE_DSG:
            full_seconds = self.full_confirm_seconds(vmax, vmin)
            if full_seconds:
                self.empty_ticks = 0
                self.full_ticks = min(self.full_ticks + 1, full_seconds * TICKS_PER_SECOND)
                if self.full_ticks >= full_seconds * TICKS_PER_SECOND:
                    self.set_soc(step_toward(self.soc, 100, CAL_STEP))
                    self.full_ticks = 0
                return self.soc != old
            self.full_ticks = max(0, self.full_ticks - 1)
        else:
            self.full_ticks = 0

        if direction == MODE_CHG:
            self.empty_ticks = 0
            return False
        if not self.voltage_allowed(vmax, vmin):
            self.empty_ticks = 0
            return False
        if self.sag_hold_blocks_calibration(vmax, vmin):
            self.empty_ticks = 0
            return False
        config = self.empty_tail_config(direction, vmin, idsg)
        if not config:
            self.empty_ticks = 0
            return False
        target, ticks = config
        self.empty_ticks += 1
        if self.empty_ticks >= ticks:
            if self.soc > target:
                self.set_soc(step_toward(self.soc, target, CAL_STEP))
            self.empty_ticks = 0
        return self.soc != old

    def full_confirm_seconds(self, vmax, vmin, v100=4180):
        vmin_min = max(0, v100 - FULL_CONFIRM_MARGIN_MV)
        vmin_fast = max(0, v100 - 30)
        if not self.voltage_allowed(vmax, vmin) or vmax < vmin_min:
            return 0
        delta = vmax - vmin
        if vmin >= vmin_fast and delta <= FULL_CONFIRM_MAX_DELTA_MV:
            return FULL_FAST_SECONDS
        if self.soc >= FULL_MIN_SOC and vmin >= vmin_min and delta <= FULL_CONFIRM_MAX_DELTA_MV:
            return FULL_SECONDS
        return 0

    def empty_current_band(self, direction, idsg):
        if direction == MODE_RELAX:
            return EMPTY_BAND_RELAX
        if idsg <= EMPTY_LIGHT_CURRENT_A10:
            return EMPTY_BAND_LIGHT
        if idsg <= EMPTY_MID_CURRENT_A10:
            return EMPTY_BAND_MID
        return EMPTY_BAND_HEAVY

    def heavy_discharge_active(self, direction, idsg):
        return direction == MODE_DSG and idsg > EMPTY_MID_CURRENT_A10

    def update_sag_hold(self, direction, idsg):
        if self.heavy_discharge_active(direction, idsg):
            self.sag_hold_ticks = SAG_HOLDOFF_SECONDS * TICKS_PER_SECOND
        elif self.sag_hold_ticks > 0:
            self.sag_hold_ticks -= 1

    def sag_hold_blocks_calibration(self, vmax, vmin):
        return self.sag_hold_ticks > 0 and self.voltage_allowed(vmax, vmin) and vmin > SAG_ALLOW_MV

    def empty_tail_config(self, direction, vmin, idsg):
        band = self.empty_current_band(direction, idsg)
        for offset, targets, ticks in EMPTY_TAIL_TABLE:
            if vmin <= EMPTY_MV + offset:
                return targets[band], max(1, ticks[band])
        return None

    def low_tail_active(self, direction, vmax, vmin, idsg):
        if direction == MODE_CHG or not self.voltage_allowed(vmax, vmin):
            return False
        if self.sag_hold_blocks_calibration(vmax, vmin):
            return False
        return self.empty_tail_config(direction, vmin, idsg) is not None

    def apply_rest_ocv(self, rest_seconds, vmax, vmin, direction=MODE_RELAX, fault=False):
        if rest_seconds < REST_OCV_SECONDS or not self.voltage_allowed(vmax, vmin, fault):
            return False
        target = interp_soc(vmin)
        if direction == MODE_CHG and target <= self.soc:
            return False
        if direction == MODE_DSG and target >= self.soc:
            return False
        old = self.soc
        self.set_soc(step_toward(self.soc, target, CAL_STEP))
        return self.soc != old

    def update_rest_timer(self, vmax, vmin):
        if self.mode != MODE_RELAX:
            self.rest_ticks = 0
            return
        self.rest_ticks = min(self.rest_ticks + 1, REST_OCV_SECONDS * TICKS_PER_SECOND)
        if self.rest_ticks >= REST_OCV_SECONDS * TICKS_PER_SECOND:
            self.apply_rest_ocv(REST_OCV_SECONDS, vmax, vmin, direction=MODE_RELAX)
            self.rest_ticks = 0

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
        ticks = DISPLAY_NORMAL_SECONDS * TICKS_PER_SECOND
        if target < self.display_soc and vmin <= EMPTY_MV + 50:
            ticks = 1 if vmin <= EMPTY_MV - 50 else DISPLAY_LOW_SECONDS * TICKS_PER_SECOND
        elif target > self.display_soc and self.mode == MODE_CHG:
            ticks = DISPLAY_CHG_SECONDS * TICKS_PER_SECOND
        self.display_ticks += 1
        if self.display_ticks >= ticks:
            self.display_soc += 1 if self.display_soc < target else -1
            self.display_ticks = 0

    def tick(self, vmax=3600, vmin=3600, ichg=0, idsg=0, fault=False):
        self.mode = self.direction(ichg, idsg)
        self.integrate(self.mode, ichg if self.mode == MODE_CHG else idsg)
        self.update_sag_hold(self.mode, idsg)
        low_tail_active = self.low_tail_active(self.mode, vmax, vmin, idsg)
        calibrated = self.apply_full_empty(self.mode, vmax, vmin, ichg, idsg)
        if not low_tail_active and not calibrated and not self.sag_hold_blocks_calibration(vmax, vmin):
            self.update_rest_timer(vmax, vmin)
        elif low_tail_active or self.sag_hold_blocks_calibration(vmax, vmin):
            self.rest_ticks = 0
        self.update_display(vmin=vmin)


def run_seconds(model, seconds, **kwargs):
    for _ in range(seconds * TICKS_PER_SECOND):
        model.tick(**kwargs)


def voltage_from_soc(soc):
    soc = max(0, min(100, soc))
    for (v1, s1), (v2, s2) in zip(OCV_TABLE, OCV_TABLE[1:]):
        if s1 >= soc >= s2:
            if s1 == s2:
                return v1
            return int((v1 * (soc - s2) + v2 * (s1 - soc)) / (s1 - s2))
    return OCV_TABLE[0][0] if soc >= OCV_TABLE[0][1] else OCV_TABLE[-1][0]


def ride_voltage(model, idsg=0, ichg=0, imbalance=4):
    base = voltage_from_soc(model.soc)
    if idsg:
        sag = min(520, idsg // 2)
        vmin = max(2400, base - sag - imbalance)
    elif ichg:
        rise = min(120, ichg // 8)
        vmin = min(4200, base + rise)
    else:
        vmin = base
    vmax = min(5000, vmin + imbalance)
    return vmax, vmin


def run_ride_segment(model, seconds, idsg=0, ichg=0, imbalance=4):
    for _ in range(seconds * TICKS_PER_SECOND):
        vmax, vmin = ride_voltage(model, idsg=idsg, ichg=ichg, imbalance=imbalance)
        model.tick(vmax=vmax, vmin=vmin, ichg=ichg, idsg=idsg)


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
    cap_full = CAP_FACTORY_AS10 * 99 // 100
    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=cap_full * 80 // 100, cycle_x100=10000))
    assert model.soc == 80
    assert model.soh == 99
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


def test_coulomb_counting_tick_is_200ms_5hz():
    model = SocModel.from_snapshot(Snapshot(soc=50, cap_now=CAP_FACTORY_AS10 * 50 // 100))
    start_cap = model.cap_now
    model.tick(vmax=3700, vmin=3700, idsg=10)
    assert start_cap - model.cap_now == 2

    model = SocModel.from_snapshot(Snapshot(soc=50, cap_now=CAP_FACTORY_AS10 * 50 // 100))
    start_cap = model.cap_now
    run_seconds(model, 1, idsg=10)
    assert start_cap - model.cap_now == 10


def test_fast_current_pulses_integrate_average_energy_at_sample_rate():
    model = SocModel.from_snapshot(Snapshot(soc=70, cap_now=CAP_FACTORY_AS10 * 70 // 100))
    start_cap = model.cap_now
    expected_delta = 0
    currents = [30, 260, 80, 420, 0, 160, 320, 40]
    for index in range(120 * TICKS_PER_SECOND):
        idsg = currents[index % len(currents)]
        vmax, vmin = ride_voltage(model, idsg=idsg, imbalance=8 if idsg >= 260 else 4)
        model.tick(vmax=vmax, vmin=vmin, idsg=idsg)
        expected_delta += idsg * PERIOD_MS // 1000
    actual_delta = start_cap - model.cap_now
    assert abs(actual_delta - expected_delta) <= 1
    assert 66 <= model.soc <= 70
    assert model.display_soc >= model.soc


def test_charge_integration_stops_display_before_full_confirm():
    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    run_seconds(model, 900, ichg=270, vmax=4100, vmin=4050)
    assert model.soc == 99
    assert model.display_soc < 100


def test_soh_maps_cycles_to_capacity_floor():
    assert soh_from_cycle(0) == 100
    assert soh_from_cycle(5000) == 100
    assert soh_from_cycle(10000) == 99
    assert soh_from_cycle(200000) == 80
    assert soh_from_cycle(300000) == 80


def test_full_confirm_is_voltage_based_and_tolerates_charge_current():
    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    run_seconds(model, FULL_SECONDS - 1, ichg=270, vmax=4180, vmin=4100)
    assert model.soc != 100
    run_seconds(model, 1, ichg=270, vmax=4180, vmin=4100)
    assert model.soc == 99
    run_seconds(model, FULL_SECONDS, ichg=270, vmax=4180, vmin=4100)
    assert model.soc == 100

    model = SocModel.from_snapshot(Snapshot(soc=99, cap_now=CAP_FACTORY_AS10 * 99 // 100))
    run_seconds(model, FULL_SECONDS, vmax=4180, vmin=4100)
    assert model.soc == 100

    model = SocModel.from_snapshot(Snapshot(soc=60, cap_now=CAP_FACTORY_AS10 * 60 // 100))
    run_seconds(model, FULL_FAST_SECONDS, ichg=270, vmax=4180, vmin=4150)
    assert model.soc == 61

    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    run_seconds(model, FULL_SECONDS + 1, ichg=270, vmax=4180, vmin=4090)
    assert model.soc != 100

    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    run_seconds(model, FULL_SECONDS + 1, ichg=270, vmax=4180, vmin=4050)
    assert model.soc != 100

    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    run_seconds(model, FULL_SECONDS + 1, ichg=270, vmax=4180, vmin=4040)
    assert model.soc != 100


def test_full_confirm_threshold_follows_configured_v100():
    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    assert model.full_confirm_seconds(vmax=3570, vmin=3570, v100=3650) == FULL_SECONDS
    assert model.full_confirm_seconds(vmax=3569, vmin=3569, v100=3650) == 0
    assert model.full_confirm_seconds(vmax=3650, vmin=3520, v100=3650) == 0


def test_empty_anchor_limits_low_voltage_tail():
    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    run_seconds(model, 5, idsg=40, vmax=3000, vmin=3000)
    assert model.soc == 25

    model = SocModel.from_snapshot(Snapshot(soc=60, cap_now=CAP_FACTORY_AS10 * 60 // 100))
    run_seconds(model, 500, idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=3400, vmin=3400)
    assert model.soc == 30

    model = SocModel.from_snapshot(Snapshot(soc=60, cap_now=CAP_FACTORY_AS10 * 60 // 100))
    run_seconds(model, 300, idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=3300, vmin=3300)
    assert 35 <= model.soc <= 36

    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    run_seconds(model, 80, idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=3100, vmin=3100)
    assert 13 <= model.soc <= 14

    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    run_seconds(model, 50, idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=3050, vmin=3050)
    assert 8 <= model.soc <= 9

    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    model.tick(idsg=40, vmax=2750, vmin=2750)
    assert model.soc == 29
    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    model.tick(idsg=40, vmax=2500, vmin=2500)
    assert model.soc == 29
    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    run_seconds(model, 15, idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=3000, vmin=3000)
    assert model.soc == 15
    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    model.tick(idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=2950, vmin=2950)
    assert model.soc == 29
    run_seconds(model, 6, idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=2950, vmin=2950)
    assert model.soc == 0


def test_rtc_rest_ocv_applies_small_bounded_step():
    model = SocModel.from_snapshot(Snapshot(soc=50, cap_now=CAP_FACTORY_AS10 * 50 // 100))
    changed = model.apply_rest_ocv(21600, vmax=3835, vmin=3835)
    assert changed
    assert model.soc == 51
    model.update_display()
    assert model.display_soc == 50
    for _ in range(5 * TICKS_PER_SECOND - 1):
        model.update_display()
    assert model.display_soc == 51


def test_ocv_correction_fault_blocking_follows_config_and_direction_errors():
    model = SocModel.from_snapshot(Snapshot(soc=50, cap_now=CAP_FACTORY_AS10 * 50 // 100))
    assert not model.apply_rest_ocv(21600, vmax=3835, vmin=3835, direction=MODE_DSG)
    assert model.soc == 50
    changed = model.apply_rest_ocv(21600, vmax=3835, vmin=3835, fault=True)
    assert changed == (not BLOCK_CALIBRATION_PROTECTION_FAULT)
    assert model.soc == (51 if not BLOCK_CALIBRATION_PROTECTION_FAULT else 50)


def test_display_smoothing_charge_and_low_voltage_down():
    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=CAP_FACTORY_AS10 * 80 // 100))
    model.display_soc = 79
    model.mode = MODE_CHG
    for _ in range(DISPLAY_NORMAL_SECONDS * TICKS_PER_SECOND - 1):
        model.update_display()
    assert model.display_soc == 79
    model.update_display()
    assert model.display_soc == 80
    model.soc = 70
    model.display_soc = 75
    for _ in range(TICKS_PER_SECOND):
        model.update_display(vmin=3050)
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


def test_low_voltage_tail_table_uses_current_bands_and_rate_limits():
    model = SocModel.from_snapshot(Snapshot(soc=40, cap_now=CAP_FACTORY_AS10 * 40 // 100))
    run_seconds(model, 19, idsg=20, vmax=3400, vmin=3400)
    assert model.soc == 40
    run_seconds(model, 1, idsg=20, vmax=3400, vmin=3400)
    assert model.soc == 39

    model = SocModel.from_snapshot(Snapshot(soc=40, cap_now=CAP_FACTORY_AS10 * 40 // 100))
    run_seconds(model, 16, idsg=EMPTY_LIGHT_CURRENT_A10 + 1, vmax=3400, vmin=3400)
    assert model.soc == 39

    model = SocModel.from_snapshot(Snapshot(soc=40, cap_now=CAP_FACTORY_AS10 * 40 // 100))
    run_seconds(model, 60, idsg=EMPTY_MID_CURRENT_A10 + 10, vmax=3400, vmin=3400)
    assert model.soc == 39


def test_auto_calibration_never_steps_more_than_one_percent():
    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    run_seconds(model, FULL_SECONDS, ichg=270, vmax=4180, vmin=4100)
    assert model.soc == 99

    model = SocModel.from_snapshot(Snapshot(soc=50, cap_now=CAP_FACTORY_AS10 * 50 // 100))
    assert model.apply_rest_ocv(21600, vmax=3835, vmin=3835)
    assert model.soc == 51

    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    model.tick(idsg=EMPTY_MID_CURRENT_A10 + 10, vmax=2950, vmin=2950)
    assert model.soc == 29

    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    model.tick(idsg=EMPTY_MID_CURRENT_A10 + 10, vmax=2500, vmin=2500)
    assert model.soc == 29


def test_heavy_discharge_sag_hold_blocks_voltage_table_until_tail():
    model = SocModel.from_snapshot(Snapshot(soc=60, cap_now=CAP_FACTORY_AS10 * 60 // 100))
    run_seconds(model, 60, idsg=EMPTY_MID_CURRENT_A10 + 10, vmax=3400, vmin=3400)
    assert model.soc >= 58
    held_soc = model.soc
    assert model.sag_hold_ticks > 0

    run_seconds(model, 20, vmax=3400, vmin=3400)
    assert model.soc == held_soc
    assert model.sag_hold_ticks > 0

    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    run_seconds(model, 6, idsg=EMPTY_MID_CURRENT_A10 + 10, vmax=2950, vmin=2950)
    assert model.soc == 0


def test_real_city_ride_profile_is_smooth_and_monotonic():
    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=CAP_FACTORY_AS10 * 80 // 100))
    previous_soc = model.soc
    profile = [
        (60, 0),
        (300, 80),
        (120, 220),
        (300, 120),
        (60, 0),
        (180, 350),
        (300, 100),
    ]
    for seconds, idsg in profile:
        run_ride_segment(model, seconds, idsg=idsg)
        assert model.soc <= previous_soc
        assert 0 <= model.display_soc <= 100
        previous_soc = model.soc
    assert 55 <= model.soc <= 70
    assert model.cap_now < CAP_FACTORY_AS10 * 80 // 100
    assert abs(model.display_soc - model.soc) <= 3


def test_hill_climb_voltage_sag_does_not_false_empty_pack():
    model = SocModel.from_snapshot(Snapshot(soc=60, cap_now=CAP_FACTORY_AS10 * 60 // 100))
    run_ride_segment(model, 180, idsg=420, imbalance=10)
    hill_soc = model.soc
    assert hill_soc >= 48
    assert model.sag_hold_ticks > 0

    run_ride_segment(model, 120, idsg=0, imbalance=4)
    assert model.soc <= hill_soc
    assert model.soc >= 47


def test_deep_ride_profile_reaches_zero_near_cutoff_voltage():
    model = SocModel.from_snapshot(Snapshot(soc=18, cap_now=CAP_FACTORY_AS10 * 18 // 100))
    min_seen = 5000
    for _ in range(900 * TICKS_PER_SECOND):
        vmax, vmin = ride_voltage(model, idsg=180, imbalance=6)
        min_seen = min(min_seen, vmin)
        model.tick(vmax=vmax, vmin=vmin, idsg=180)
        if model.soc == 0:
            break
    assert min_seen <= 3050
    assert model.soc == 0
    assert model.display_soc <= 5


def test_charge_after_ride_stays_below_full_until_voltage_anchor():
    model = SocModel.from_snapshot(Snapshot(soc=88, cap_now=CAP_FACTORY_AS10 * 88 // 100))
    run_seconds(model, 600, ichg=270, vmax=4100, vmin=4050)
    assert model.soc == 99
    assert model.display_soc < 100
    run_seconds(model, FULL_SECONDS, ichg=270, vmax=4180, vmin=4100)
    assert model.soc == 100


def main():
    tests = [
        test_invalid_snapshot_defaults_to_60_percent,
        test_invalid_snapshot_uses_valid_startup_ocv,
        test_valid_snapshot_restores_capacity_and_cycle_soh,
        test_set_soc_once_syncs_internal_capacity_and_display,
        test_discharge_integration_reduces_soc_and_counts_cycle_fraction,
        test_coulomb_counting_tick_is_200ms_5hz,
        test_fast_current_pulses_integrate_average_energy_at_sample_rate,
        test_charge_integration_stops_display_before_full_confirm,
        test_soh_maps_cycles_to_capacity_floor,
        test_full_confirm_is_voltage_based_and_tolerates_charge_current,
        test_full_confirm_threshold_follows_configured_v100,
        test_empty_anchor_limits_low_voltage_tail,
        test_rtc_rest_ocv_applies_small_bounded_step,
        test_ocv_correction_fault_blocking_follows_config_and_direction_errors,
        test_display_smoothing_charge_and_low_voltage_down,
        test_fixed_and_zero_overlay_do_not_change_internal_soc,
        test_low_voltage_tail_table_uses_current_bands_and_rate_limits,
        test_auto_calibration_never_steps_more_than_one_percent,
        test_heavy_discharge_sag_hold_blocks_voltage_table_until_tail,
        test_real_city_ride_profile_is_smooth_and_monotonic,
        test_hill_climb_voltage_sag_does_not_false_empty_pack,
        test_deep_ride_profile_reaches_zero_near_cutoff_voltage,
        test_charge_after_ride_stays_below_full_until_voltage_anchor,
    ]
    for test in tests:
        test()
        print('PASS {0}'.format(test.__name__))
    print('SOC replay tests passed: {0}'.format(len(tests)))


if __name__ == '__main__':
    main()
