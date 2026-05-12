#include "bms_app.h"

#include <string.h>

static bool voltage_valid_for_protection(const bms_sample_t *sample)
{
    return sample->vcell_min_mv >= 2000u &&
           sample->vcell_max_mv >= sample->vcell_min_mv &&
           sample->vcell_max_mv <= 5000u &&
           sample->vcell_delta_mv <= 1000u;
}

static void update_latched_fault(uint32_t *faults, uint32_t bit, bool set_condition, bool release_condition)
{
    if (set_condition) {
        *faults |= bit;
    } else if (release_condition) {
        *faults &= ~bit;
    }
}

void bms_protection_init(bms_protection_state_t *protection)
{
    memset(protection, 0, sizeof(*protection));
    protection->charge_mos_on = true;
    protection->discharge_mos_on = true;
}

void bms_protection_update(bms_protection_state_t *protection, const bms_config_t *config, const bms_sample_t *sample)
{
    uint32_t faults = protection->active_faults;
    const bool voltage_ok = voltage_valid_for_protection(sample);

    update_latched_fault(
        &faults,
        BMS_FAULT_VOLTAGE_INVALID,
        !voltage_ok,
        voltage_ok);

    if (voltage_ok) {
        update_latched_fault(
            &faults,
            BMS_FAULT_CELL_OVP,
            sample->vcell_max_mv >= config->cell_ovp_mv,
            sample->vcell_max_mv <= config->cell_ovp_release_mv);
        update_latched_fault(
            &faults,
            BMS_FAULT_CELL_UVP,
            sample->vcell_min_mv <= config->cell_uvp_mv,
            sample->vcell_min_mv >= config->cell_uvp_release_mv);
    }

    update_latched_fault(
        &faults,
        BMS_FAULT_CHG_OCP,
        sample->ichg_a10 >= config->charge_ocp_a10,
        sample->ichg_a10 < (uint16_t)(config->charge_ocp_a10 / 2u));
    update_latched_fault(
        &faults,
        BMS_FAULT_DSG_OCP,
        sample->idsg_a10 >= config->discharge_ocp_a10,
        sample->idsg_a10 < (uint16_t)(config->discharge_ocp_a10 / 2u));
    update_latched_fault(
        &faults,
        BMS_FAULT_TEMP_HIGH,
        sample->temp_max_c >= config->temp_high_c,
        sample->temp_max_c <= config->temp_high_release_c);

    protection->active_faults = faults;
    protection->charge_mos_on = (faults & (BMS_FAULT_CELL_OVP | BMS_FAULT_CHG_OCP | BMS_FAULT_TEMP_HIGH | BMS_FAULT_VOLTAGE_INVALID)) == 0u;
    protection->discharge_mos_on = (faults & (BMS_FAULT_CELL_UVP | BMS_FAULT_DSG_OCP | BMS_FAULT_TEMP_HIGH | BMS_FAULT_VOLTAGE_INVALID)) == 0u;
}
