#!/usr/bin/env python3
"""Host-side replay tests for the simplified e-bike SOC model.

The model mirrors the rewritten SocEnhance.c decisions that matter for storage,
anchoring, SOH mapping, and SOC publication. It intentionally avoids STM32
bindings so it can run in CI or during local review.
"""

import random
import re
import os
import ast
from dataclasses import dataclass
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOC_ENHANCE_SOURCE = PROJECT_ROOT / '103 + 309/Project/Source/SocEnhance.c'
PROJECT_CONFIG_SOURCE = PROJECT_ROOT / '103 + 309/Project/Source/conf/Project_Config.h'
try:
    PROJECT_CONFIG_TEXT = PROJECT_CONFIG_SOURCE.read_text(encoding='utf-8', errors='ignore')
except FileNotFoundError:
    PROJECT_CONFIG_TEXT = ''


def project_config_int(name, default):
    match = re.search(r'^\s*#define\s+{0}\s+(\d+)\b'.format(re.escape(name)),
                      PROJECT_CONFIG_TEXT,
                      re.MULTILINE)
    return int(match.group(1)) if match else default


def env_int(name, default):
    value = os.environ.get(name)
    return int(value) if value is not None else default


TICKS_PER_SECOND = 5
PERIOD_MS = 200
CURRENT_ENTER_A10 = 2
MA_PER_A10 = 100
MAMS_PER_AS10 = 100000
CONFIG_BOARD_SELF_CONSUMPTION_MA = project_config_int('PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA', 30)
BOARD_SELF_CONSUMPTION_MA = env_int('SOC_TEST_BOARD_SELF_CONSUMPTION_MA', 30)
DEFAULT_SOC = 60
CAP_A10 = 270
CAP_FACTORY_AS10 = CAP_A10 * 3600
SOH_MIN = 80
SOH_STEP_CYCLES = 100
FULL_SECONDS = project_config_int('PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS', 15)
FULL_FAST_SECONDS = project_config_int('PROJECT_CFG_SOC_FULL_CONFIRM_FAST_SECONDS', 5)
FULL_MIN_SOC = project_config_int('PROJECT_CFG_SOC_FULL_CONFIRM_MIN_SOC_PERCENT', 95)
FULL_CONFIRM_MARGIN_MV = project_config_int('PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV', 80)
FULL_FAST_MARGIN_MV = project_config_int('PROJECT_CFG_SOC_FULL_CONFIRM_FAST_MARGIN_MV', 30)
FULL_CONFIRM_MAX_DELTA_MV = project_config_int('PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV', 120)
FULL_CONFIRM_MIN_VMAX_MV = 4180
EMPTY_MV = 3000
REST_OCV_SECONDS = project_config_int('PROJECT_CFG_SOC_REST_OCV_SECONDS', 1800)
EMPTY_LIGHT_CURRENT_A10 = max(CURRENT_ENTER_A10, (CAP_A10 + 4) // 5)
EMPTY_MID_CURRENT_A10 = max(CURRENT_ENTER_A10, (CAP_A10 + 1) // 2)
CAL_STEP = project_config_int('PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT', 1)
EMPTY_TAIL_START_OFFSET_MV = project_config_int('PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV', 400)
SAG_HOLDOFF_SECONDS = project_config_int('PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS', 30)
SAG_ALLOW_MV = EMPTY_MV + project_config_int('PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV', 50)
REBOUND_BOOT_HOLDOFF_SECONDS = 300
REST_MAX_DELTA_MV = 200
REST_STABLE_DELTA_MV = 30
PRE_LONG_REST_PROBE_SECONDS = max(1, REST_OCV_SECONDS // 3)
LONG_REST_DOWN_STEP_SECONDS = project_config_int('PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS', 1800)
REST_STABLE_LIMIT_SECONDS = REST_OCV_SECONDS
VALID_MIN_MV = project_config_int('PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV', 2000)
VALID_MAX_MV = project_config_int('PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV', 5000)
VALID_MAX_DELTA_MV = 300

DELAY_SOC_TEST_TICKS = 5 * 60
LOW_TAIL_STEP_SECONDS = DELAY_SOC_TEST_TICKS // TICKS_PER_SECOND

EMPTY_BAND_RELAX = 0
EMPTY_BAND_LIGHT = 1
EMPTY_BAND_MID = 2
EMPTY_BAND_HEAVY = 3

EMPTY_TAIL_TABLE = [
    (-50, (0, 0, 0, 0), (DELAY_SOC_TEST_TICKS,) * 4),
    (-25, (0, 0, 0, 0), (DELAY_SOC_TEST_TICKS,) * 4),
    (0, (0, 0, 0, 0), (DELAY_SOC_TEST_TICKS,) * 4),
    (50, (4, 5, 8, 12), (DELAY_SOC_TEST_TICKS,) * 4),
    (100, (8, 10, 14, 18), (DELAY_SOC_TEST_TICKS,) * 4),
    (200, (12, 14, 20, 25), (DELAY_SOC_TEST_TICKS,) * 4),
    (300, (14, 18, 25, 32), (DELAY_SOC_TEST_TICKS,) * 4),
    (400, (18, 22, 30, 40), (DELAY_SOC_TEST_TICKS,) * 4),
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
    rebound_hold: bool = False


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


def trunc_div(numerator, denominator):
    if numerator >= 0:
        return numerator // denominator
    return -((-numerator) // denominator)


def soh_from_cycle(cycle_x100):
    drop = (cycle_x100 // 100) // SOH_STEP_CYCLES
    return max(SOH_MIN, 100 - drop)


@dataclass
class SocModel:
    soc: int = DEFAULT_SOC
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
    stable_rest_ticks: int = 0
    long_rest_down_ticks: int = 0
    rest_ref_vmin: int = 0
    rest_ref_vmax: int = 0
    rest_down_target: int = 0
    rest_down_valid: bool = False
    sag_hold_ticks: int = 0
    rebound_hold: bool = False
    full_anchor: bool = False
    board_self_consumption_ma: int = BOARD_SELF_CONSUMPTION_MA

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
            return model
        model.cycle_x100 = snapshot.cycle_x100
        model.recalc_full()
        model.cycle_acc_as10 = snapshot.cycle_acc_as10 % (model.cap_factory // 100)
        if snapshot.cap_now or snapshot.soc == 0:
            model.cap_now = min(snapshot.cap_now, model.cap_full)
            model.soc = model.soc_from_cap()
        else:
            model.set_soc(snapshot.soc)
        if snapshot.rebound_hold:
            model.rebound_hold = True
            model.sag_hold_ticks = REBOUND_BOOT_HOLDOFF_SECONDS * TICKS_PER_SECOND
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

    def net_current_ma(self, ichg, idsg):
        return (ichg - idsg) * MA_PER_A10

    def direction(self, ichg, idsg):
        net_current = self.net_current_ma(ichg, idsg)
        if net_current >= CURRENT_ENTER_A10 * MA_PER_A10:
            return MODE_CHG
        if net_current <= -(CURRENT_ENTER_A10 * MA_PER_A10):
            return MODE_DSG
        return MODE_RELAX

    def voltage_allowed(self, vmax, vmin):
        return VALID_MIN_MV <= vmin <= vmax <= VALID_MAX_MV and (vmax - vmin) <= VALID_MAX_DELTA_MV

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

    def integrate_current_ma(self, ichg, idsg):
        return self.net_current_ma(ichg, idsg) - self.board_self_consumption_ma

    def integrate(self, direction, ichg, idsg):
        current_ma = self.integrate_current_ma(ichg, idsg)
        self.last_mode = direction
        if current_ma == 0:
            return
        acc_mams = current_ma * PERIOD_MS + self.remainder_ms
        delta = trunc_div(acc_mams, MAMS_PER_AS10)
        self.remainder_ms = acc_mams - (delta * MAMS_PER_AS10)
        if delta == 0:
            return
        if delta > 0:
            self.cap_now = min(self.cap_full, self.cap_now + delta)
        else:
            self.full_anchor = False
            self.add_cycle_capacity(-delta)
            self.cap_now = max(0, self.cap_now + delta)
        self.soc = self.soc_from_cap()
        if delta > 0 and not self.full_anchor and self.soc >= 100:
            self.soc = 99
            self.cap_now = self.cap_full * 99 // 100

    def clear_rest_down_target(self):
        self.rest_down_valid = False
        self.rest_down_target = 0
        self.long_rest_down_ticks = 0

    def set_rest_down_target(self, target):
        if target >= self.soc:
            self.clear_rest_down_target()
            return
        if not self.rest_down_valid or self.rest_down_target != target:
            self.rest_down_target = target
            self.rest_down_valid = True
            self.long_rest_down_ticks = 0

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
        vmin_fast = max(0, v100 - FULL_FAST_MARGIN_MV)
        if not self.voltage_allowed(vmax, vmin) or vmax <= FULL_CONFIRM_MIN_VMAX_MV or vmax < vmin_min:
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
            self.rebound_hold = True
        elif self.sag_hold_ticks > 0:
            self.sag_hold_ticks -= 1
            if self.sag_hold_ticks == 0:
                self.rebound_hold = False
        else:
            self.rebound_hold = False

    def sag_hold_blocks_calibration(self, vmax, vmin):
        return self.sag_hold_ticks > 0 and self.voltage_allowed(vmax, vmin) and vmin > SAG_ALLOW_MV

    def empty_tail_config(self, direction, vmin, idsg):
        if vmin > EMPTY_MV + EMPTY_TAIL_START_OFFSET_MV:
            return None
        band = self.empty_current_band(direction, idsg)
        for offset, targets, ticks in EMPTY_TAIL_TABLE:
            if vmin <= EMPTY_MV + offset:
                target = targets[band]
                tick_count = max(1, ticks[band])
                return target, tick_count
        return None

    def low_tail_active(self, direction, vmax, vmin, idsg):
        if direction == MODE_CHG or not self.voltage_allowed(vmax, vmin):
            return False
        if self.sag_hold_blocks_calibration(vmax, vmin):
            return False
        return self.empty_tail_config(direction, vmin, idsg) is not None

    def apply_ocv_target_step(self, target, vmax, vmin, direction=MODE_RELAX):
        if not self.voltage_allowed(vmax, vmin):
            return False
        if self.sag_hold_blocks_calibration(vmax, vmin):
            return False
        if direction == MODE_CHG and target <= self.soc:
            return False
        if direction == MODE_DSG and target >= self.soc:
            return False
        old = self.soc
        self.set_soc(step_toward(self.soc, target, CAL_STEP))
        return self.soc != old

    def apply_ocv_step(self, vmax, vmin, direction=MODE_RELAX):
        return self.apply_ocv_target_step(interp_soc(vmin), vmax, vmin, direction=direction)

    def apply_long_rest_down_step(self, vmax, vmin, delta_ticks=1):
        if (not self.rest_down_valid or self.rest_down_target >= self.soc or
                self.rest_ticks < REST_OCV_SECONDS * TICKS_PER_SECOND):
            self.long_rest_down_ticks = 0
            return False
        limit = LONG_REST_DOWN_STEP_SECONDS * TICKS_PER_SECOND
        self.long_rest_down_ticks = min(limit, self.long_rest_down_ticks + delta_ticks)
        if self.long_rest_down_ticks < limit:
            return False
        old_target = self.rest_down_target
        changed = self.apply_ocv_target_step(old_target, vmax, vmin, direction=MODE_RELAX)
        self.long_rest_down_ticks = 0
        if self.soc == old_target:
            self.clear_rest_down_target()
        return changed

    def reset_rest_confidence(self):
        self.rest_ticks = 0
        self.stable_rest_ticks = 0
        self.clear_rest_down_target()
        self.rest_ref_vmin = 0
        self.rest_ref_vmax = 0

    def rest_voltage_stable(self, vmax, vmin):
        if not self.voltage_allowed(vmax, vmin):
            return False
        if (vmax - vmin) > REST_MAX_DELTA_MV:
            return False
        if self.sag_hold_blocks_calibration(vmax, vmin):
            return False
        if self.rest_ref_vmin == 0 or self.rest_ref_vmax == 0:
            self.rest_ref_vmin = vmin
            self.rest_ref_vmax = vmax
            return True
        if abs(vmin - self.rest_ref_vmin) <= REST_STABLE_DELTA_MV and abs(vmax - self.rest_ref_vmax) <= REST_STABLE_DELTA_MV:
            return True
        self.rest_ref_vmin = vmin
        self.rest_ref_vmax = vmax
        return False

    def update_rest_timer(self, vmax, vmin):
        if self.mode != MODE_RELAX:
            self.reset_rest_confidence()
            return
        self.rest_ticks = min(self.rest_ticks + 1, REST_OCV_SECONDS * TICKS_PER_SECOND)
        if self.rest_voltage_stable(vmax, vmin):
            self.stable_rest_ticks = min(self.stable_rest_ticks + 1,
                                         REST_STABLE_LIMIT_SECONDS * TICKS_PER_SECOND)
        else:
            self.stable_rest_ticks = 0
            self.clear_rest_down_target()
        if (self.rest_ticks >= REST_OCV_SECONDS * TICKS_PER_SECOND and
                self.stable_rest_ticks >= REST_STABLE_LIMIT_SECONDS * TICKS_PER_SECOND):
            self.set_rest_down_target(interp_soc(vmin))
        self.apply_long_rest_down_step(vmax, vmin)

    def tick(self, vmax=3600, vmin=3600, ichg=0, idsg=0):
        self.mode = self.direction(ichg, idsg)
        self.integrate(self.mode, ichg, idsg)
        self.update_sag_hold(self.mode, idsg)
        low_tail_active = self.low_tail_active(self.mode, vmax, vmin, idsg)
        calibrated = self.apply_full_empty(self.mode, vmax, vmin, ichg, idsg)
        if not low_tail_active and not calibrated and not self.sag_hold_blocks_calibration(vmax, vmin):
            self.update_rest_timer(vmax, vmin)
        elif low_tail_active or self.sag_hold_blocks_calibration(vmax, vmin):
            self.reset_rest_confidence()


def run_seconds(model, seconds, **kwargs):
    for _ in range(seconds * TICKS_PER_SECOND):
        model.tick(**kwargs)


def model_with_self_consumption(soc, self_ma):
    model = SocModel.from_snapshot(Snapshot(soc=soc, cap_now=CAP_FACTORY_AS10 * soc // 100))
    model.board_self_consumption_ma = self_ma
    return model


def self_consumption_delta_as10(self_ma, seconds):
    return self_ma * seconds * 1000 // MAMS_PER_AS10


def assert_model_invariants(model):
    assert 0 <= model.soc <= 100
    assert SOH_MIN <= model.soh <= 100
    assert 0 <= model.cap_now <= model.cap_full <= model.cap_factory
    assert 0 <= model.cycle_acc_as10 < max(1, model.cap_factory // 100)
    assert model.rest_ticks <= REST_OCV_SECONDS * TICKS_PER_SECOND
    assert model.stable_rest_ticks <= REST_STABLE_LIMIT_SECONDS * TICKS_PER_SECOND
    assert model.long_rest_down_ticks <= LONG_REST_DOWN_STEP_SECONDS * TICKS_PER_SECOND
    assert model.sag_hold_ticks <= REBOUND_BOOT_HOLDOFF_SECONDS * TICKS_PER_SECOND


def idsg_for_band(band):
    if band == EMPTY_BAND_RELAX:
        return 0
    if band == EMPTY_BAND_LIGHT:
        return CURRENT_ENTER_A10
    if band == EMPTY_BAND_MID:
        return EMPTY_LIGHT_CURRENT_A10 + 1
    return EMPTY_MID_CURRENT_A10 + 1


def direction_for_band(band):
    return MODE_RELAX if band == EMPTY_BAND_RELAX else MODE_DSG


def c_source_text():
    return SOC_ENHANCE_SOURCE.read_text(encoding='utf-8')


def active_c_source_text():
    text = c_source_text()
    text = re.sub(r'#if\s+0\b.*?#else', '', text, flags=re.S)
    text = re.sub(r'^\s*#endif\s*$', '', text, flags=re.MULTILINE)
    return text


def parse_c_number(token):
    token = token.strip()
    macro = re.search(r'^\s*#define\s+{0}\s+(.+?)\s*$'.format(re.escape(token)),
                      active_c_source_text(),
                      re.MULTILINE)
    if macro:
        return parse_c_number(macro.group(1).split('//', 1)[0])
    token = re.sub(r'\(\s*[A-Za-z_][A-Za-z0-9_]*\s*\)', '', token)
    token = re.sub(r'(?<=\d)[UuLl]+', '', token)
    if not re.fullmatch(r'[\d\s+\-*/%()]+', token):
        raise ValueError(token)
    parsed = ast.parse(token, mode='eval')
    for node in ast.walk(parsed):
        if not isinstance(node, (
            ast.Expression, ast.BinOp, ast.UnaryOp, ast.Constant,
            ast.Add, ast.Sub, ast.Mult, ast.Div, ast.FloorDiv,
            ast.Mod, ast.USub, ast.UAdd,
        )):
            raise ValueError(token)
    return int(eval(compile(parsed, '<c-int>', 'eval'), {'__builtins__': {}}, {}))


def parse_c_tail_table(name):
    match = re.search(r'static const SOC_EMPTY_TAIL_RULE\s+' + name +
                      r'\[\]\s*=\s*\{(.*?)\};', active_c_source_text(), re.S)
    assert match, name
    rows = []
    for offset, targets, ticks in re.findall(r'\{\s*([^,]+),\s*\{([^}]*)\},\s*\{([^}]*)\}\s*\}',
                                             match.group(1)):
        rows.append((
            parse_c_number(offset),
            tuple(parse_c_number(item) for item in targets.split(',')),
            tuple(parse_c_number(item) for item in ticks.split(',')),
        ))
    return rows


def parse_c_ternary_ocv_table():
    match = re.search(r'const UINT16 SocTable_TernaryLi\[[^\]]+\]\s*=\s*\{(.*?)\};',
                      c_source_text(), re.S)
    assert match
    values = [int(item) for item in re.findall(r'\d+', match.group(1))]
    return list(zip(values[0::2], values[1::2]))


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
    assert model.soh == 100


def test_invalid_snapshot_uses_valid_startup_ocv():
    model = SocModel.from_snapshot(Snapshot(valid=False, vmax=3835, vmin=3835))
    assert model.soc == 70


def test_valid_snapshot_restores_capacity_and_cycle_soh():
    cap_full = CAP_FACTORY_AS10 * 99 // 100
    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=cap_full * 80 // 100, cycle_x100=10000))
    assert model.soc == 80
    assert model.soh == 99
    assert model.cap_full == cap_full


def test_set_soc_once_syncs_capacity():
    model = SocModel.from_snapshot(Snapshot(valid=False))
    model.set_soc(35)
    assert model.soc == 35
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


def test_board_self_consumption_integrates_during_relax():
    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=CAP_FACTORY_AS10 * 80 // 100))
    start_cap = model.cap_now
    run_seconds(model, 600, vmax=3835, vmin=3835)
    expected_delta = BOARD_SELF_CONSUMPTION_MA * 600 * 1000 // MAMS_PER_AS10
    assert model.mode == MODE_RELAX
    assert start_cap - model.cap_now == expected_delta


def test_board_self_consumption_matrix_during_relax_high_voltage():
    for self_ma in (0, 30, 1000, CONFIG_BOARD_SELF_CONSUMPTION_MA):
        model = model_with_self_consumption(80, self_ma)
        start_cap = model.cap_now
        run_seconds(model, 3600, vmax=4050, vmin=4050)
        expected_delta = self_consumption_delta_as10(self_ma, 3600)
        assert model.mode == MODE_RELAX
        assert start_cap - model.cap_now == expected_delta
        if self_ma == 0:
            assert model.soc == 80
        elif self_ma >= 1000:
            assert model.soc < 80


def test_full_voltage_anchor_can_mask_self_consumption():
    model = model_with_self_consumption(99, 1000)
    run_seconds(model, FULL_FAST_SECONDS * 4, vmax=4181, vmin=4181)
    assert model.soc == 100
    assert model.cap_now == model.cap_full


def test_board_self_consumption_applies_to_charge_and_discharge_current():
    charge_model = model_with_self_consumption(70, 1000)
    charge_start = charge_model.cap_now
    run_seconds(charge_model, 100, vmax=3835, vmin=3835, ichg=2)
    assert charge_model.mode == MODE_CHG
    assert charge_start - charge_model.cap_now == self_consumption_delta_as10(800, 100)

    discharge_model = model_with_self_consumption(70, 1000)
    discharge_start = discharge_model.cap_now
    run_seconds(discharge_model, 100, vmax=3835, vmin=3835, idsg=2)
    assert discharge_model.mode == MODE_DSG
    assert discharge_start - discharge_model.cap_now == self_consumption_delta_as10(1200, 100)


def test_fast_current_pulses_integrate_average_energy_at_sample_rate():
    model = SocModel.from_snapshot(Snapshot(soc=70, cap_now=CAP_FACTORY_AS10 * 70 // 100))
    start_cap = model.cap_now
    expected_rem_mams = 0
    expected_delta = 0
    currents = [30, 260, 80, 420, 0, 160, 320, 40]
    for index in range(120 * TICKS_PER_SECOND):
        idsg = currents[index % len(currents)]
        vmax, vmin = ride_voltage(model, idsg=idsg, imbalance=8 if idsg >= 260 else 4)
        model.tick(vmax=vmax, vmin=vmin, idsg=idsg)
        signed_ma = model.integrate_current_ma(0, idsg)
        if signed_ma == 0:
            continue
        acc_mams = signed_ma * PERIOD_MS + expected_rem_mams
        delta = trunc_div(acc_mams, MAMS_PER_AS10)
        if delta < 0:
            expected_delta += -delta
        expected_rem_mams = acc_mams - (delta * MAMS_PER_AS10)
    actual_delta = start_cap - model.cap_now
    assert abs(actual_delta - expected_delta) <= 1
    assert 66 <= model.soc <= 70


def test_direction_thresholds_and_conflict_resolution():
    model = SocModel.from_snapshot(Snapshot(soc=50, cap_now=CAP_FACTORY_AS10 * 50 // 100))
    model.tick(ichg=CURRENT_ENTER_A10 - 1, idsg=CURRENT_ENTER_A10 - 1)
    assert model.mode == MODE_RELAX

    model.tick(ichg=CURRENT_ENTER_A10, idsg=CURRENT_ENTER_A10)
    assert model.mode == MODE_RELAX

    model = SocModel.from_snapshot(Snapshot(soc=50, cap_now=CAP_FACTORY_AS10 * 50 // 100))
    model.tick(ichg=CURRENT_ENTER_A10, idsg=CURRENT_ENTER_A10 + 1)
    assert model.mode == MODE_RELAX
    expected_ma = -(MA_PER_A10 + BOARD_SELF_CONSUMPTION_MA)
    assert model.remainder_ms == expected_ma * PERIOD_MS

    model = SocModel.from_snapshot(Snapshot(soc=50, cap_now=CAP_FACTORY_AS10 * 50 // 100))
    model.tick(ichg=CURRENT_ENTER_A10, idsg=CURRENT_ENTER_A10 + 2)
    assert model.mode == MODE_DSG
    expected_ma = -((2 * MA_PER_A10) + BOARD_SELF_CONSUMPTION_MA)
    assert model.remainder_ms == expected_ma * PERIOD_MS


def test_charge_integration_stays_below_full_before_confirm():
    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    run_seconds(model, 900, ichg=270, vmax=4100, vmin=4050)
    assert model.soc == 99


def test_soh_maps_cycles_to_capacity_floor():
    assert soh_from_cycle(0) == 100
    assert soh_from_cycle(5000) == 100
    assert soh_from_cycle(10000) == 99
    assert soh_from_cycle(200000) == 80
    assert soh_from_cycle(300000) == 80


def test_ocv_table_is_monotonic_and_exact_points_match():
    previous_soc = 100
    for mv, soc in OCV_TABLE:
        assert interp_soc(mv) == soc
        assert soc <= previous_soc
        previous_soc = soc

    previous = 100
    for mv in range(4200, 2900, -5):
        now = interp_soc(mv)
        assert 0 <= now <= 100
        assert now <= previous
        previous = now


def test_python_model_tables_match_c_source():
    assert parse_c_ternary_ocv_table() == OCV_TABLE
    assert parse_c_tail_table('s_empty_tail_table') == EMPTY_TAIL_TABLE


def test_full_confirm_is_voltage_based_and_tolerates_charge_current():
    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    run_seconds(model, FULL_SECONDS + 1, ichg=270, vmax=4180, vmin=4100)
    assert model.soc != 100
    assert model.full_ticks == 0

    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    run_seconds(model, FULL_SECONDS - 1, ichg=270, vmax=4181, vmin=4100)
    assert model.soc != 100
    run_seconds(model, 1, ichg=270, vmax=4181, vmin=4100)
    assert model.soc == 99
    run_seconds(model, FULL_SECONDS, ichg=270, vmax=4181, vmin=4100)
    assert model.soc == 100

    model = SocModel.from_snapshot(Snapshot(soc=99, cap_now=CAP_FACTORY_AS10 * 99 // 100))
    run_seconds(model, FULL_SECONDS, vmax=4181, vmin=4100)
    assert model.soc == 100

    model = SocModel.from_snapshot(Snapshot(soc=60, cap_now=CAP_FACTORY_AS10 * 60 // 100))
    run_seconds(model, FULL_FAST_SECONDS, ichg=270, vmax=4181, vmin=4150)
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


def test_full_confirm_requires_vmax_above_4180_before_configured_v100():
    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    assert model.full_confirm_seconds(vmax=4180, vmin=4180, v100=3650) == 0
    assert model.full_confirm_seconds(vmax=4181, vmin=4181, v100=3650) == FULL_FAST_SECONDS
    assert model.full_confirm_seconds(vmax=4181, vmin=3520, v100=3650) == 0


def test_full_confirm_counter_decrements_instead_of_resetting():
    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    run_seconds(model, FULL_SECONDS - 1, ichg=270, vmax=4181, vmin=4100)
    assert model.full_ticks == (FULL_SECONDS - 1) * TICKS_PER_SECOND

    model.tick(ichg=270, vmax=4000, vmin=3990)
    assert model.full_ticks == (FULL_SECONDS - 1) * TICKS_PER_SECOND - 1
    run_seconds(model, 2, ichg=270, vmax=4181, vmin=4100)
    assert model.soc == 99


def test_empty_anchor_limits_low_voltage_tail():
    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    run_seconds(model, LOW_TAIL_STEP_SECONDS, idsg=40, vmax=3000, vmin=3000)
    assert model.soc == 29

    model = SocModel.from_snapshot(Snapshot(soc=60, cap_now=CAP_FACTORY_AS10 * 60 // 100))
    run_seconds(model, 500, idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=3400, vmin=3400)
    assert model.soc == 52

    model = SocModel.from_snapshot(Snapshot(soc=60, cap_now=CAP_FACTORY_AS10 * 60 // 100))
    run_seconds(model, 300, idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=3300, vmin=3300)
    assert model.soc == 55

    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    run_seconds(model, 80, idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=3100, vmin=3100)
    assert model.soc == 29

    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    run_seconds(model, 50, idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=3050, vmin=3050)
    assert model.soc == 30

    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    model.tick(idsg=40, vmax=2750, vmin=2750)
    assert model.soc == 30
    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    model.tick(idsg=40, vmax=2500, vmin=2500)
    assert model.soc == 30
    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    run_seconds(model, 15, idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=3000, vmin=3000)
    assert model.soc == 30
    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    model.tick(idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=2950, vmin=2950)
    assert model.soc == 30
    run_seconds(model, LOW_TAIL_STEP_SECONDS, idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=2950, vmin=2950)
    assert model.soc == 29


def test_low_voltage_tail_table_uses_current_bands_and_rate_limits():
    model = SocModel.from_snapshot(Snapshot(soc=40, cap_now=CAP_FACTORY_AS10 * 40 // 100))
    assert model.empty_tail_config(MODE_DSG, 3400, 20) == (22, DELAY_SOC_TEST_TICKS)
    for _ in range(DELAY_SOC_TEST_TICKS - 1):
        model.tick(idsg=20, vmax=3400, vmin=3400)
    assert model.soc == 40
    model.tick(idsg=20, vmax=3400, vmin=3400)
    assert model.soc == 39

    model = SocModel.from_snapshot(Snapshot(soc=40, cap_now=CAP_FACTORY_AS10 * 40 // 100))
    run_seconds(model, 1, idsg=EMPTY_LIGHT_CURRENT_A10 + 1, vmax=3400, vmin=3400)
    assert model.soc == 40
    run_seconds(model, LOW_TAIL_STEP_SECONDS, idsg=EMPTY_LIGHT_CURRENT_A10 + 1, vmax=3400, vmin=3400)
    assert model.soc == 39

    model = SocModel.from_snapshot(Snapshot(soc=40, cap_now=CAP_FACTORY_AS10 * 40 // 100))
    run_seconds(model, 1, idsg=EMPTY_MID_CURRENT_A10 + 10, vmax=3400, vmin=3400)
    assert model.soc == 40
    assert model.sag_hold_ticks > 0


def test_low_voltage_tail_table_matrix_targets_rates_and_no_upward_pull():
    for offset, targets, ticks in EMPTY_TAIL_TABLE:
        vcell = EMPTY_MV + offset
        for band in (EMPTY_BAND_RELAX, EMPTY_BAND_LIGHT, EMPTY_BAND_MID, EMPTY_BAND_HEAVY):
            direction = direction_for_band(band)
            idsg = idsg_for_band(band)
            model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=CAP_FACTORY_AS10 * 80 // 100))
            assert model.empty_tail_config(direction, vcell, idsg) == (targets[band], max(1, ticks[band]))

            model.soc = min(100, targets[band] + 5)
            model.cap_now = model.cap_full * model.soc // 100
            for _ in range(max(1, ticks[band]) - 1):
                assert not model.apply_full_empty(direction, vcell, vcell, 0, idsg)
            changed = model.apply_full_empty(direction, vcell, vcell, 0, idsg)
            assert changed == (model.soc == targets[band] + 4)

            model = SocModel.from_snapshot(Snapshot(soc=max(0, targets[band] - 2),
                                                    cap_now=CAP_FACTORY_AS10 * max(0, targets[band] - 2) // 100))
            for _ in range(max(1, ticks[band]) + 1):
                model.apply_full_empty(direction, vcell, vcell, 0, idsg)
            assert model.soc == max(0, targets[band] - 2)


def test_auto_calibration_never_steps_more_than_one_percent():
    previous_soc: int

    model = SocModel.from_snapshot(Snapshot(soc=98, cap_now=CAP_FACTORY_AS10 * 98 // 100))
    run_seconds(model, FULL_SECONDS, ichg=270, vmax=4181, vmin=4100)
    assert model.soc == 99

    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=CAP_FACTORY_AS10 * 80 // 100))
    assert model.apply_ocv_target_step(interp_soc(3835), vmax=3835, vmin=3835)
    assert model.soc == 79

    model = SocModel.from_snapshot(Snapshot(soc=30, cap_now=CAP_FACTORY_AS10 * 30 // 100))
    previous_soc = model.soc
    for _ in range(30):
        model.tick(idsg=EMPTY_LIGHT_CURRENT_A10 + 10, vmax=2950, vmin=2950)
        assert previous_soc - model.soc in (0, 1)
        previous_soc = model.soc

    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=CAP_FACTORY_AS10 * 80 // 100))
    run_seconds(model, 1, idsg=20, vmax=3500, vmin=3500)
    assert model.soc == 80


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
    assert model.soc == 30
    run_seconds(model, LOW_TAIL_STEP_SECONDS, idsg=EMPTY_MID_CURRENT_A10 + 10, vmax=2950, vmin=2950)
    assert model.soc <= 29


def test_short_stable_rest_does_not_latch_ocv_target():
    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=CAP_FACTORY_AS10 * 80 // 100))
    run_seconds(model, PRE_LONG_REST_PROBE_SECONDS - 1, vmax=3835, vmin=3835)
    assert model.soc == 80
    run_seconds(model, 1, vmax=3835, vmin=3835)
    assert model.soc == 80
    assert not model.rest_down_valid


def test_short_rest_ocv_upward_gap_is_ignored_during_charge():
    model = SocModel.from_snapshot(Snapshot(soc=50, cap_now=CAP_FACTORY_AS10 * 50 // 100))
    run_seconds(model, PRE_LONG_REST_PROBE_SECONDS, vmax=3835, vmin=3835)
    assert model.soc == 50
    assert not model.rest_down_valid
    run_seconds(model, PRE_LONG_REST_PROBE_SECONDS, ichg=CURRENT_ENTER_A10, vmax=3835, vmin=3835)
    assert model.soc == 50


def test_short_rest_ocv_downward_gap_is_not_consumed_during_discharge():
    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=CAP_FACTORY_AS10 * 80 // 100))
    run_seconds(model, PRE_LONG_REST_PROBE_SECONDS, vmax=3835, vmin=3835)
    before_discharge = model.soc
    assert not model.rest_down_valid
    run_seconds(model, PRE_LONG_REST_PROBE_SECONDS, idsg=CURRENT_ENTER_A10, vmax=3835, vmin=3835)
    assert model.soc == before_discharge


def test_unstable_short_rest_does_not_ocv_calibrate():
    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=CAP_FACTORY_AS10 * 80 // 100))
    for _ in range(5):
        run_seconds(model, 60, vmax=3810, vmin=3810)
        run_seconds(model, 60, vmax=3770, vmin=3770)
    assert model.soc == 80


def test_unstable_long_rest_waits_for_voltage_convergence():
    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=CAP_FACTORY_AS10 * 80 // 100))
    for index in range(9):
        vcell = 3835 if (index % 2) == 0 else 3770
        run_seconds(model, 200, vmax=vcell, vmin=vcell)
    assert model.soc == 80

    run_seconds(model, REST_OCV_SECONDS - 200, vmax=3835, vmin=3835)
    assert model.soc == 80
    assert not model.rest_down_valid
    run_seconds(model, 1, vmax=3835, vmin=3835)
    assert model.soc == 80
    assert model.rest_down_valid
    assert model.rest_down_target == 70
    run_seconds(model, LONG_REST_DOWN_STEP_SECONDS - 2, vmax=3835, vmin=3835)
    assert model.soc == 80
    run_seconds(model, 1, vmax=3835, vmin=3835)
    assert model.soc == 79


def test_mid_voltage_light_load_no_longer_limits_high_soc():
    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=CAP_FACTORY_AS10 * 80 // 100))
    for _ in range(20):
        model.tick(idsg=20, vmax=3500, vmin=3500)
    assert model.soc == 80


def test_short_rest_rejects_imbalance_and_restarts_after_voltage_jump():
    model = SocModel.from_snapshot(Snapshot(soc=50, cap_now=CAP_FACTORY_AS10 * 50 // 100))
    run_seconds(model, PRE_LONG_REST_PROBE_SECONDS + 1, vmax=3835, vmin=3600)
    assert model.soc == 50

    run_seconds(model, PRE_LONG_REST_PROBE_SECONDS, vmax=3835, vmin=3835)
    assert model.stable_rest_ticks >= PRE_LONG_REST_PROBE_SECONDS * TICKS_PER_SECOND
    model.tick(vmax=3870, vmin=3870)
    assert model.stable_rest_ticks == 0
    assert not model.rest_down_valid


def test_persisted_rebound_hold_blocks_startup_voltage_correction():
    model = SocModel.from_snapshot(Snapshot(soc=80,
                                            cap_now=CAP_FACTORY_AS10 * 80 // 100,
                                            rebound_hold=True))
    run_seconds(model, 120, vmax=3500, vmin=3500)
    assert model.soc == 80
    assert model.sag_hold_ticks > 0


def test_rebound_hold_expires_then_allows_voltage_correction():
    model = SocModel.from_snapshot(Snapshot(soc=80,
                                            cap_now=CAP_FACTORY_AS10 * 80 // 100,
                                            rebound_hold=True))
    run_seconds(model, REBOUND_BOOT_HOLDOFF_SECONDS - 1, vmax=3500, vmin=3500)
    assert model.soc == 80
    assert model.rebound_hold

    run_seconds(model, 1, vmax=3500, vmin=3500)
    assert not model.rebound_hold
    run_seconds(model, 90, vmax=3500, vmin=3500)
    assert model.soc == 80


def test_long_storage_stable_voltage_converges_gradually_without_jump():
    model = SocModel.from_snapshot(Snapshot(soc=90, cap_now=CAP_FACTORY_AS10 * 90 // 100))
    previous = model.soc
    for _ in range(3600 * TICKS_PER_SECOND):
        model.tick(vmax=3835, vmin=3835)
        assert previous - model.soc in (0, 1)
        previous = model.soc
        assert_model_invariants(model)
    assert model.soc == 89


def test_long_rest_ocv_slowly_reduces_soc_above_low_tail():
    model = SocModel.from_snapshot(Snapshot(soc=80, cap_now=CAP_FACTORY_AS10 * 80 // 100))
    rest_down_start_seconds = REST_OCV_SECONDS
    run_seconds(model, rest_down_start_seconds + LONG_REST_DOWN_STEP_SECONDS - 1,
                vmax=3725, vmin=3725)
    assert model.soc == 80
    run_seconds(model, 1, vmax=3725, vmin=3725)
    assert model.soc == 79


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
        previous_soc = model.soc
    assert 55 <= model.soc <= 70
    assert model.cap_now < CAP_FACTORY_AS10 * 80 // 100


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


def test_charge_after_ride_stays_below_full_until_voltage_anchor():
    model = SocModel.from_snapshot(Snapshot(soc=88, cap_now=CAP_FACTORY_AS10 * 88 // 100))
    run_seconds(model, 600, ichg=270, vmax=4100, vmin=4050)
    assert model.soc == 99
    run_seconds(model, FULL_SECONDS, ichg=270, vmax=4181, vmin=4100)
    assert model.soc == 100


def test_voltage_error_matrix_never_calibrates_or_clears_state_wrongly():
    bad_samples = [
        (1999, 1999),
        (5001, 5001),
        (3600, 3700),
        (4700, 3600),
        (0, 0),
    ]
    for vmax, vmin in bad_samples:
        model = SocModel.from_snapshot(Snapshot(soc=70, cap_now=CAP_FACTORY_AS10 * 70 // 100))
        assert not model.apply_ocv_step(vmax, vmin)
        assert not model.low_tail_active(MODE_RELAX, vmax, vmin, 0)
        run_seconds(model, PRE_LONG_REST_PROBE_SECONDS + 1, vmax=vmax, vmin=vmin)
        assert model.soc == 70
        assert_model_invariants(model)


def test_randomized_operating_matrix_preserves_core_invariants():
    random.seed(103309)
    model = SocModel.from_snapshot(Snapshot(soc=65, cap_now=CAP_FACTORY_AS10 * 65 // 100))
    for tick_index in range(20000):
        phase = tick_index % 17
        if phase in (0, 1, 2):
            ichg = random.choice([0, 4, 20, 80, 270])
            idsg = random.choice([0, 1, 3])
        elif phase in (3, 4, 5, 6, 7, 8):
            ichg = 0
            idsg = random.choice([4, 20, EMPTY_LIGHT_CURRENT_A10, EMPTY_LIGHT_CURRENT_A10 + 1,
                                  EMPTY_MID_CURRENT_A10, EMPTY_MID_CURRENT_A10 + 1, 420])
        else:
            ichg = 0
            idsg = 0

        if random.random() < 0.08:
            vmax, vmin = random.choice([(0, 0), (1990, 1990), (3600, 3700), (4700, 3600)])
        else:
            center = random.choice([2950, 3000, 3050, 3100, 3200, 3300, 3400,
                                    3500, 3600, 3650, 3700, 3835, 4100, 4180])
            delta = random.choice([0, 5, 30, 120, 199])
            vmin = max(2000, min(5000, center))
            vmax = min(5000, vmin + delta)

        old_soc = model.soc
        model.tick(vmax=vmax, vmin=vmin, ichg=ichg, idsg=idsg)
        assert_model_invariants(model)
        if ichg == 0 and idsg == 0 and model.sag_hold_ticks == 0 and model.low_tail_active(model.mode, vmax, vmin, idsg):
            assert model.soc <= old_soc


def main():
    tests = [
        test_invalid_snapshot_defaults_to_60_percent,
        test_invalid_snapshot_uses_valid_startup_ocv,
        test_valid_snapshot_restores_capacity_and_cycle_soh,
        test_set_soc_once_syncs_capacity,
        test_discharge_integration_reduces_soc_and_counts_cycle_fraction,
        test_coulomb_counting_tick_is_200ms_5hz,
        test_board_self_consumption_integrates_during_relax,
        test_board_self_consumption_matrix_during_relax_high_voltage,
        test_full_voltage_anchor_can_mask_self_consumption,
        test_board_self_consumption_applies_to_charge_and_discharge_current,
        test_fast_current_pulses_integrate_average_energy_at_sample_rate,
        test_direction_thresholds_and_conflict_resolution,
        test_charge_integration_stays_below_full_before_confirm,
        test_soh_maps_cycles_to_capacity_floor,
        test_ocv_table_is_monotonic_and_exact_points_match,
        test_python_model_tables_match_c_source,
        test_full_confirm_is_voltage_based_and_tolerates_charge_current,
        test_full_confirm_requires_vmax_above_4180_before_configured_v100,
        test_full_confirm_counter_decrements_instead_of_resetting,
        test_empty_anchor_limits_low_voltage_tail,
        test_low_voltage_tail_table_uses_current_bands_and_rate_limits,
        test_low_voltage_tail_table_matrix_targets_rates_and_no_upward_pull,
        test_auto_calibration_never_steps_more_than_one_percent,
        test_heavy_discharge_sag_hold_blocks_voltage_table_until_tail,
        test_short_stable_rest_does_not_latch_ocv_target,
        test_short_rest_ocv_upward_gap_is_ignored_during_charge,
        test_short_rest_ocv_downward_gap_is_not_consumed_during_discharge,
        test_unstable_short_rest_does_not_ocv_calibrate,
        test_unstable_long_rest_waits_for_voltage_convergence,
        test_mid_voltage_light_load_no_longer_limits_high_soc,
        test_short_rest_rejects_imbalance_and_restarts_after_voltage_jump,
        test_persisted_rebound_hold_blocks_startup_voltage_correction,
        test_rebound_hold_expires_then_allows_voltage_correction,
        test_long_storage_stable_voltage_converges_gradually_without_jump,
        test_long_rest_ocv_slowly_reduces_soc_above_low_tail,
        test_real_city_ride_profile_is_smooth_and_monotonic,
        test_hill_climb_voltage_sag_does_not_false_empty_pack,
        test_deep_ride_profile_reaches_zero_near_cutoff_voltage,
        test_charge_after_ride_stays_below_full_until_voltage_anchor,
        test_voltage_error_matrix_never_calibrates_or_clears_state_wrongly,
        test_randomized_operating_matrix_preserves_core_invariants,
    ]
    for test in tests:
        test()
        print('PASS {0}'.format(test.__name__))
    print('SOC replay tests passed: {0}'.format(len(tests)))


if __name__ == '__main__':
    main()
