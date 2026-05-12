#include "bms_app.h"

#include <stdlib.h>
#include <string.h>

#define BMS_SOC_FULL_FAST_MS 5000u
#define BMS_SOC_FULL_NORMAL_MS 15000u
#define BMS_SOC_FULL_MIN_SOC 95u
#define BMS_SOC_FULL_FAST_MARGIN_MV 30u
#define BMS_SOC_FULL_NORMAL_MARGIN_MV 80u
#define BMS_SOC_FULL_MAX_DELTA_MV 120u
#define BMS_SOC_REST_MIN_MS 300000u
#define BMS_SOC_REST_TARGET_MS 600000u
#define BMS_SOC_LONG_REST_DOWN_MS 1800000u
#define BMS_SOC_REST_STABLE_DELTA_MV 30u
#define BMS_SOC_REST_MAX_DELTA_MV 200u
#define BMS_SOC_SAG_HOLD_MS 30000u
#define BMS_SOC_DISPLAY_NORMAL_MS 5000u
#define BMS_SOC_DISPLAY_LOW_MS 1000u
#define BMS_SOC_DISPLAY_EMPTY_MS 200u
#define BMS_SOC_MAX_PERCENT 100u
#define BMS_SOC_DEFAULT_START 60u

typedef struct {
    uint16_t max_mv;
    uint8_t target[4];
    uint32_t period_ms[4];
} bms_low_row_t;

typedef struct {
    uint16_t max_mv;
    uint8_t target[3];
    uint32_t period_ms[3];
} bms_mid_row_t;

static const bms_soc_table_point_t k_default_ocv[BMS_SOC_TABLE_POINTS] = {
    {4160u, 100u}, {4100u, 95u}, {4050u, 90u}, {3995u, 85u}, {3935u, 80u},
    {3880u, 75u},  {3835u, 70u}, {3795u, 65u}, {3760u, 60u}, {3725u, 55u},
    {3695u, 50u},  {3670u, 45u}, {3645u, 40u}, {3615u, 35u}, {3585u, 30u},
    {3555u, 25u},  {3525u, 20u}, {3480u, 15u}, {3400u, 10u}, {3250u, 5u},
    {3000u, 0u},
};

static const bms_low_row_t k_low_table[] = {
    {2950u, {0u, 0u, 0u, 0u}, {200u, 200u, 200u, 200u}},
    {2975u, {0u, 0u, 0u, 0u}, {1000u, 1000u, 200u, 200u}},
    {3000u, {0u, 0u, 0u, 0u}, {2000u, 1000u, 1000u, 1000u}},
    {3050u, {4u, 5u, 8u, 12u}, {4000u, 3000u, 2000u, 1600u}},
    {3100u, {8u, 10u, 14u, 18u}, {7000u, 6000u, 5000u, 4000u}},
    {3200u, {12u, 14u, 20u, 25u}, {12000u, 10000u, 8000u, 6000u}},
    {3300u, {14u, 18u, 25u, 32u}, {18000u, 15000u, 12000u, 9000u}},
    {3400u, {18u, 22u, 30u, 40u}, {24000u, 20000u, 16000u, 12000u}},
};

static const bms_mid_row_t k_mid_table[] = {
    {3500u, {25u, 32u, 42u}, {90000u, 90000u, 120000u}},
    {3600u, {35u, 42u, 50u}, {120000u, 120000u, 150000u}},
    {3650u, {45u, 50u, 58u}, {150000u, 150000u, 180000u}},
    {3700u, {55u, 60u, 255u}, {180000u, 180000u, 0u}},
};

static uint8_t clamp_u8(uint32_t value, uint8_t min_value, uint8_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return (uint8_t)value;
}

static int32_t clamp_i32(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static bool voltage_valid(const bms_sample_t *sample)
{
    return sample->vcell_min_mv >= 2000u &&
           sample->vcell_max_mv >= sample->vcell_min_mv &&
           sample->vcell_max_mv <= 5000u &&
           sample->vcell_delta_mv <= 1000u &&
           !sample->protection_fault;
}

static int32_t cap_from_percent(const bms_soc_state_t *soc, uint8_t percent)
{
    return (soc->cap_full_as10 * (int32_t)percent) / 100;
}

static uint8_t percent_from_cap(const bms_soc_state_t *soc)
{
    if (soc->cap_full_as10 <= 0) {
        return 0u;
    }
    return clamp_u8((uint32_t)((soc->cap_now_as10 * 100 + soc->cap_full_as10 / 2) / soc->cap_full_as10), 0u, 100u);
}

static void set_internal_soc(bms_soc_state_t *soc, uint8_t percent)
{
    soc->soc = clamp_u8(percent, 0u, 100u);
    soc->cap_now_as10 = cap_from_percent(soc, soc->soc);
}

static void step_soc_down(bms_soc_state_t *soc)
{
    if (soc->soc > 0u) {
        set_internal_soc(soc, (uint8_t)(soc->soc - 1u));
    }
}

static void step_soc_up(bms_soc_state_t *soc)
{
    if (soc->soc < 100u) {
        set_internal_soc(soc, (uint8_t)(soc->soc + 1u));
    }
}

static uint8_t ocv_lookup(const bms_soc_state_t *soc, uint16_t vcell_mv)
{
    size_t i;

    if (vcell_mv >= soc->ocv_table[0].voltage_mv) {
        return soc->ocv_table[0].soc_percent;
    }

    for (i = 1u; i < BMS_SOC_TABLE_POINTS; ++i) {
        const bms_soc_table_point_t high = soc->ocv_table[i - 1u];
        const bms_soc_table_point_t low = soc->ocv_table[i];
        if (vcell_mv >= low.voltage_mv) {
            const uint16_t dv = (uint16_t)(high.voltage_mv - low.voltage_mv);
            const uint16_t ds = (uint16_t)(high.soc_percent - low.soc_percent);
            if (dv == 0u) {
                return low.soc_percent;
            }
            return (uint8_t)(low.soc_percent + ((uint32_t)(vcell_mv - low.voltage_mv) * ds + dv / 2u) / dv);
        }
    }

    return soc->ocv_table[BMS_SOC_TABLE_POINTS - 1u].soc_percent;
}

static void update_soh_and_full_capacity(bms_soc_state_t *soc)
{
    const uint32_t full_cycles = soc->cycle_x100 / 100u;
    uint8_t soh = 100u;
    if (full_cycles >= 100u) {
        soh = (uint8_t)(100u - (full_cycles / 100u));
    }
    if (soh < 80u) {
        soh = 80u;
    }
    soc->soh = soh;
    soc->cap_full_as10 = (soc->cap_factory_as10 * (int32_t)soc->soh) / 100;
    soc->cap_now_as10 = clamp_i32(soc->cap_now_as10, 0, soc->cap_full_as10);
    soc->soc = percent_from_cap(soc);
}

static uint8_t load_tier(const bms_config_t *config, const bms_sample_t *sample, bms_soc_mode_t mode)
{
    const uint16_t light_limit = (uint16_t)(config->capacity_ah10 / 5u);
    const uint16_t medium_limit = (uint16_t)(config->capacity_ah10 / 2u);

    if (mode == BMS_SOC_MODE_RELAX) {
        return 0u;
    }
    if (sample->idsg_a10 <= light_limit) {
        return 1u;
    }
    if (sample->idsg_a10 <= medium_limit) {
        return 2u;
    }
    return 3u;
}

static void update_mode_and_sag(bms_soc_state_t *soc, const bms_config_t *config, const bms_sample_t *sample, uint32_t elapsed_ms)
{
    const int32_t net_a10 = (int32_t)sample->ichg_a10 - (int32_t)sample->idsg_a10;
    const uint16_t heavy_limit = (uint16_t)(config->capacity_ah10 / 2u);

    if (net_a10 >= (int32_t)config->current_deadband_a10) {
        soc->mode = BMS_SOC_MODE_CHARGE;
    } else if (net_a10 <= -(int32_t)config->current_deadband_a10) {
        soc->mode = BMS_SOC_MODE_DISCHARGE;
    } else {
        soc->mode = BMS_SOC_MODE_RELAX;
    }

    if (sample->idsg_a10 > heavy_limit) {
        soc->sag_hold_ms = BMS_SOC_SAG_HOLD_MS;
    } else if (soc->sag_hold_ms > elapsed_ms) {
        soc->sag_hold_ms -= elapsed_ms;
    } else {
        soc->sag_hold_ms = 0u;
    }
}

static void integrate_current(bms_soc_state_t *soc, uint32_t elapsed_ms, const bms_sample_t *sample)
{
    const int32_t net_a10 = (int32_t)sample->ichg_a10 - (int32_t)sample->idsg_a10;
    int32_t delta_as10;
    const int32_t cycle_unit_as10 = soc->cap_factory_as10 / 100;

    if (soc->mode == BMS_SOC_MODE_RELAX) {
        return;
    }

    if (net_a10 > 0) {
        const uint64_t acc = (uint64_t)net_a10 * elapsed_ms + soc->rem_charge_ms;
        delta_as10 = (int32_t)(acc / 1000u);
        soc->rem_charge_ms = (uint32_t)(acc % 1000u);
        soc->cap_now_as10 = clamp_i32(soc->cap_now_as10 + delta_as10, 0, soc->cap_full_as10);
        if (!soc->full_anchor && percent_from_cap(soc) >= 100u) {
            soc->cap_now_as10 = cap_from_percent(soc, 99u);
        }
    } else {
        const uint32_t discharge_a10 = (uint32_t)(-net_a10);
        const uint64_t acc = (uint64_t)discharge_a10 * elapsed_ms + soc->rem_discharge_ms;
        delta_as10 = (int32_t)(acc / 1000u);
        soc->rem_discharge_ms = (uint32_t)(acc % 1000u);
        soc->cap_now_as10 = clamp_i32(soc->cap_now_as10 - delta_as10, 0, soc->cap_full_as10);
        soc->discharge_acc_as10 += delta_as10;
        while (cycle_unit_as10 > 0 && soc->discharge_acc_as10 >= cycle_unit_as10) {
            soc->discharge_acc_as10 -= cycle_unit_as10;
            soc->cycle_x100 += 1u;
        }
    }

    update_soh_and_full_capacity(soc);
}

static void update_full_anchor(bms_soc_state_t *soc, const bms_config_t *config, const bms_sample_t *sample, uint32_t elapsed_ms)
{
    const bool base = soc->mode != BMS_SOC_MODE_DISCHARGE &&
                      voltage_valid(sample) &&
                      sample->vcell_max_mv >= (uint16_t)(config->full_cell_mv - BMS_SOC_FULL_NORMAL_MARGIN_MV) &&
                      sample->vcell_delta_mv <= BMS_SOC_FULL_MAX_DELTA_MV;
    const bool fast = base && sample->vcell_min_mv >= (uint16_t)(config->full_cell_mv - BMS_SOC_FULL_FAST_MARGIN_MV);
    const bool normal = base && soc->soc >= BMS_SOC_FULL_MIN_SOC &&
                        sample->vcell_min_mv >= (uint16_t)(config->full_cell_mv - BMS_SOC_FULL_NORMAL_MARGIN_MV);

    if (fast) {
        soc->full_fast_ms += elapsed_ms;
    } else if (soc->full_fast_ms > elapsed_ms) {
        soc->full_fast_ms -= elapsed_ms;
    } else {
        soc->full_fast_ms = 0u;
    }

    if (normal) {
        soc->full_normal_ms += elapsed_ms;
    } else if (soc->full_normal_ms > elapsed_ms) {
        soc->full_normal_ms -= elapsed_ms;
    } else {
        soc->full_normal_ms = 0u;
    }

    if (soc->full_fast_ms >= BMS_SOC_FULL_FAST_MS || soc->full_normal_ms >= BMS_SOC_FULL_NORMAL_MS) {
        if (soc->soc < 100u) {
            step_soc_up(soc);
        }
        if (soc->soc >= 100u) {
            soc->full_anchor = true;
            soc->cap_now_as10 = soc->cap_full_as10;
        }
        soc->full_fast_ms = 0u;
        soc->full_normal_ms = 0u;
    }
}

static void update_low_voltage(bms_soc_state_t *soc, const bms_config_t *config, const bms_sample_t *sample, uint32_t elapsed_ms)
{
    const uint8_t tier = load_tier(config, sample, soc->mode);
    size_t i;

    if (soc->mode == BMS_SOC_MODE_CHARGE || !voltage_valid(sample)) {
        soc->low_voltage_ms = 0u;
        return;
    }

    if (soc->sag_hold_ms > 0u && sample->vcell_min_mv > (uint16_t)(config->empty_cell_mv + 50u)) {
        soc->low_voltage_ms = 0u;
        return;
    }

    for (i = 0u; i < sizeof(k_low_table) / sizeof(k_low_table[0]); ++i) {
        const bms_low_row_t *row = &k_low_table[i];
        if (sample->vcell_min_mv <= row->max_mv) {
            const uint8_t target = row->target[tier];
            const uint32_t period = row->period_ms[tier];
            if (soc->soc > target) {
                soc->low_voltage_ms += elapsed_ms;
                if (soc->low_voltage_ms >= period) {
                    step_soc_down(soc);
                    soc->low_voltage_ms = 0u;
                }
            } else {
                soc->low_voltage_ms = 0u;
            }
            return;
        }
    }

    soc->low_voltage_ms = 0u;
}

static void update_mid_voltage(bms_soc_state_t *soc, const bms_config_t *config, const bms_sample_t *sample, uint32_t elapsed_ms)
{
    const uint8_t tier = load_tier(config, sample, soc->mode);
    size_t i;

    if (soc->mode == BMS_SOC_MODE_CHARGE ||
        tier == 3u ||
        !voltage_valid(sample) ||
        sample->vcell_delta_mv > BMS_SOC_REST_MAX_DELTA_MV ||
        sample->vcell_min_mv <= (uint16_t)(config->empty_cell_mv + 400u) ||
        soc->sag_hold_ms > 0u) {
        soc->mid_voltage_ms = 0u;
        return;
    }

    for (i = 0u; i < sizeof(k_mid_table) / sizeof(k_mid_table[0]); ++i) {
        const bms_mid_row_t *row = &k_mid_table[i];
        if (sample->vcell_min_mv <= row->max_mv) {
            const uint8_t target = row->target[tier];
            const uint32_t period = row->period_ms[tier];
            if (target != 255u && soc->soc > target) {
                soc->mid_voltage_ms += elapsed_ms;
                if (soc->mid_voltage_ms >= period) {
                    step_soc_down(soc);
                    soc->mid_voltage_ms = 0u;
                }
            } else {
                soc->mid_voltage_ms = 0u;
            }
            return;
        }
    }

    soc->mid_voltage_ms = 0u;
}

static bool rest_window_stable(bms_soc_state_t *soc, const bms_sample_t *sample)
{
    const uint16_t min_delta = (uint16_t)abs((int)sample->vcell_min_mv - (int)soc->rest_ref_min_mv);
    const uint16_t max_delta = (uint16_t)abs((int)sample->vcell_max_mv - (int)soc->rest_ref_max_mv);
    return min_delta <= BMS_SOC_REST_STABLE_DELTA_MV && max_delta <= BMS_SOC_REST_STABLE_DELTA_MV;
}

static void update_rest_ocv(bms_soc_state_t *soc, const bms_sample_t *sample, uint32_t elapsed_ms)
{
    if (soc->mode != BMS_SOC_MODE_RELAX ||
        !voltage_valid(sample) ||
        sample->vcell_delta_mv > BMS_SOC_REST_MAX_DELTA_MV ||
        soc->sag_hold_ms > 0u) {
        soc->rest_ref_valid = false;
        soc->rest_stable_ms = 0u;
        soc->rest_target_ms = 0u;
        soc->long_rest_down_ms = 0u;
        return;
    }

    if (!soc->rest_ref_valid) {
        soc->rest_ref_min_mv = sample->vcell_min_mv;
        soc->rest_ref_max_mv = sample->vcell_max_mv;
        soc->rest_ref_valid = true;
        soc->rest_stable_ms = 0u;
        soc->rest_target_ms = 0u;
        return;
    }

    if (!rest_window_stable(soc, sample)) {
        soc->rest_ref_min_mv = sample->vcell_min_mv;
        soc->rest_ref_max_mv = sample->vcell_max_mv;
        soc->rest_stable_ms = 0u;
        soc->rest_target_ms = 0u;
        return;
    }

    soc->rest_stable_ms += elapsed_ms;
    if (soc->rest_stable_ms >= BMS_SOC_REST_MIN_MS) {
        soc->rest_target_ms += elapsed_ms;
        if (soc->rest_target_ms >= BMS_SOC_REST_TARGET_MS) {
            soc->deferred_ocv_target = ocv_lookup(soc, sample->vcell_min_mv);
            soc->deferred_ocv_valid = true;
            soc->rest_target_ms = 0u;
        }
    }

    if (soc->deferred_ocv_valid && soc->deferred_ocv_target < soc->soc) {
        soc->long_rest_down_ms += elapsed_ms;
        if (soc->long_rest_down_ms >= BMS_SOC_LONG_REST_DOWN_MS) {
            step_soc_down(soc);
            soc->long_rest_down_ms = 0u;
        }
    } else {
        soc->long_rest_down_ms = 0u;
    }
}

static void consume_deferred_target(bms_soc_state_t *soc, uint32_t elapsed_ms)
{
    if (!soc->deferred_ocv_valid || soc->mode == BMS_SOC_MODE_RELAX) {
        soc->deferred_consume_ms = 0u;
        return;
    }

    if ((soc->mode == BMS_SOC_MODE_CHARGE && soc->deferred_ocv_target <= soc->soc) ||
        (soc->mode == BMS_SOC_MODE_DISCHARGE && soc->deferred_ocv_target >= soc->soc)) {
        soc->deferred_consume_ms = 0u;
        return;
    }

    soc->deferred_consume_ms += elapsed_ms;
    if (soc->deferred_consume_ms >= BMS_SOC_REST_TARGET_MS) {
        if (soc->deferred_ocv_target > soc->soc) {
            step_soc_up(soc);
        } else if (soc->deferred_ocv_target < soc->soc) {
            step_soc_down(soc);
        }
        soc->deferred_consume_ms = 0u;
        if (soc->deferred_ocv_target == soc->soc) {
            soc->deferred_ocv_valid = false;
        }
    }
}

static void update_display(bms_soc_state_t *soc, const bms_config_t *config, const bms_sample_t *sample, uint32_t elapsed_ms)
{
    uint32_t period = BMS_SOC_DISPLAY_NORMAL_MS;

    if (sample->vcell_min_mv <= (uint16_t)(config->empty_cell_mv - 50u)) {
        period = BMS_SOC_DISPLAY_EMPTY_MS;
    } else if (sample->vcell_min_mv <= (uint16_t)(config->empty_cell_mv + 50u) && soc->display_soc > soc->soc) {
        period = BMS_SOC_DISPLAY_LOW_MS;
    }

    if (soc->display_soc == soc->soc) {
        soc->display_ms = 0u;
        return;
    }

    soc->display_ms += elapsed_ms;
    if (soc->display_ms >= period) {
        if (soc->display_soc < soc->soc) {
            soc->display_soc += 1u;
        } else {
            soc->display_soc -= 1u;
        }
        soc->display_ms = 0u;
    }
}

bms_config_t bms_config_default(void)
{
    bms_config_t config;
    config.capacity_ah10 = 270u;
    config.cycle_count = 3u;
    config.full_cell_mv = 4180u;
    config.empty_cell_mv = 3000u;
    config.cell_count = 10u;
    config.current_deadband_a10 = 2u;
    config.rtc_idle_with_can_s = 1u;
    config.rtc_idle_without_can_s = 10u;
    config.idle_before_rtc_s = 10u;
    return config;
}

bms_sample_t bms_sample_default(void)
{
    bms_sample_t sample;
    memset(&sample, 0, sizeof(sample));
    sample.cell_count = 10u;
    sample.vcell_min_mv = 3700u;
    sample.vcell_max_mv = 3700u;
    sample.vcell_delta_mv = 0u;
    sample.pack_mv = 37000u;
    return sample;
}

void bms_soc_init(bms_soc_state_t *soc, const bms_config_t *config, const bms_snapshot_t *snapshot)
{
    memset(soc, 0, sizeof(*soc));
    memcpy(soc->ocv_table, k_default_ocv, sizeof(k_default_ocv));
    soc->cap_factory_as10 = (int32_t)config->capacity_ah10 * 3600;
    soc->cycle_x100 = (uint32_t)config->cycle_count * 100u;
    soc->soh = 100u;
    update_soh_and_full_capacity(soc);

    if (snapshot != NULL && bms_storage_validate(snapshot)) {
        soc->cap_full_as10 = (int32_t)snapshot->cap_full_as10;
        soc->cap_now_as10 = clamp_i32((int32_t)snapshot->cap_now_as10, 0, soc->cap_full_as10);
        soc->cycle_x100 = snapshot->cycle_x100;
        soc->discharge_acc_as10 = (int32_t)snapshot->discharge_acc_as10;
        soc->soc = clamp_u8(snapshot->soc, 0u, 100u);
        soc->display_soc = clamp_u8(snapshot->display_soc, 0u, 100u);
        soc->soh = clamp_u8(snapshot->soh, 80u, 100u);
        if ((snapshot->flags & 0x0001u) != 0u) {
            soc->sag_hold_ms = 300000u;
        }
        update_soh_and_full_capacity(soc);
        return;
    }

    set_internal_soc(soc, BMS_SOC_DEFAULT_START);
    soc->display_soc = soc->soc;
}

void bms_soc_update(bms_soc_state_t *soc, const bms_config_t *config, const bms_sample_t *sample, uint32_t elapsed_ms)
{
    if (elapsed_ms == 0u) {
        return;
    }

    update_mode_and_sag(soc, config, sample, elapsed_ms);
    integrate_current(soc, elapsed_ms, sample);
    update_full_anchor(soc, config, sample, elapsed_ms);
    update_mid_voltage(soc, config, sample, elapsed_ms);
    update_low_voltage(soc, config, sample, elapsed_ms);
    update_rest_ocv(soc, sample, elapsed_ms);
    consume_deferred_target(soc, elapsed_ms);
    update_display(soc, config, sample, elapsed_ms);
}

void bms_soc_set_once(bms_soc_state_t *soc, uint8_t soc_percent)
{
    set_internal_soc(soc, clamp_u8(soc_percent, 0u, 100u));
    soc->display_soc = soc->soc;
    soc->display_ms = 0u;
    soc->deferred_ocv_valid = false;
}

bms_soc_report_t bms_soc_report(const bms_soc_state_t *soc)
{
    bms_soc_report_t report;
    report.soc = soc->soc;
    report.display_soc = soc->display_soc;
    report.soh = soc->soh;
    report.capacity_now_ah100 = (uint16_t)clamp_i32(soc->cap_now_as10 / 360, 0, 65535);
    report.capacity_full_ah100 = (uint16_t)clamp_i32(soc->cap_full_as10 / 360, 0, 65535);
    report.capacity_factory_ah100 = (uint16_t)clamp_i32(soc->cap_factory_as10 / 360, 0, 65535);
    report.cycle_count = (uint16_t)(soc->cycle_x100 / 100u);
    report.full_anchor = soc->full_anchor;
    report.deferred_ocv_valid = soc->deferred_ocv_valid;
    report.deferred_ocv_target = soc->deferred_ocv_target;
    report.mode = soc->mode;
    return report;
}

bms_snapshot_t bms_soc_make_snapshot(const bms_soc_state_t *soc, uint32_t sequence)
{
    bms_snapshot_t snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.magic = 0x42534F43ul;
    snapshot.version = 2u;
    snapshot.soc = soc->soc;
    snapshot.display_soc = soc->display_soc;
    snapshot.soh = soc->soh;
    snapshot.flags = soc->sag_hold_ms > 0u ? 0x0001u : 0u;
    snapshot.cap_now_as10 = (uint32_t)soc->cap_now_as10;
    snapshot.cap_full_as10 = (uint32_t)soc->cap_full_as10;
    snapshot.cycle_x100 = soc->cycle_x100;
    snapshot.discharge_acc_as10 = (uint32_t)soc->discharge_acc_as10;
    snapshot.sequence = sequence;
    snapshot.crc = 0u;
    return snapshot;
}
