#include "bms_app.h"

#include <string.h>

static uint16_t clamp_word(uint32_t value)
{
    return value > 65535u ? 65535u : (uint16_t)value;
}

bool bms_comm_write_single(bms_app_t *app, uint16_t address, uint16_t value)
{
    if (address == BMS_ADDR_SET_ONCE_SOC && value <= 100u) {
        bms_soc_set_once(&app->soc, (uint8_t)value);
        (void)bms_app_save_snapshot(app);
        return true;
    }

    if (address == BMS_ADDR_IAP_CONNECT) {
        return bms_iap_request(&app->iap, value);
    }

    return false;
}

bool bms_comm_write_block(bms_app_t *app, uint16_t address, const uint16_t *values, size_t count)
{
    size_t i;

    if (values == NULL) {
        return false;
    }

    if (address >= BMS_ADDR_SOC_TABLE_START &&
        address <= BMS_ADDR_SOC_TABLE_END &&
        count <= (size_t)(BMS_ADDR_SOC_TABLE_END - address + 1u)) {
        const size_t offset = (size_t)(address - BMS_ADDR_SOC_TABLE_START);
        for (i = 0u; i < count; ++i) {
            const size_t word_index = offset + i;
            const size_t point = word_index / 2u;
            if (point >= BMS_SOC_TABLE_POINTS) {
                return false;
            }
            if ((word_index % 2u) == 0u) {
                app->soc.ocv_table[point].voltage_mv = values[i];
            } else if (values[i] <= 100u) {
                app->soc.ocv_table[point].soc_percent = (uint8_t)values[i];
            } else {
                return false;
            }
        }
        return true;
    }

    if (address >= BMS_ADDR_SOC_PARAM_START &&
        address <= BMS_ADDR_SOC_PARAM_END &&
        count <= (size_t)(BMS_ADDR_SOC_PARAM_END - address + 1u)) {
        for (i = 0u; i < count; ++i) {
            switch ((uint16_t)(address + i)) {
            case 0x2318u:
                app->config.capacity_ah10 = values[i] == 0u ? bms_config_default().capacity_ah10 : values[i];
                break;
            case 0x2319u:
                app->config.cycle_count = values[i];
                break;
            case 0x231Au:
                app->config.full_cell_mv = values[i] == 0u ? bms_config_default().full_cell_mv : values[i];
                break;
            case 0x231Bu:
                app->config.empty_cell_mv = values[i] == 0u ? bms_config_default().empty_cell_mv : values[i];
                break;
            default:
                return false;
            }
        }
        return true;
    }

    return false;
}

bool bms_comm_read_d000(const bms_app_t *app, uint16_t *words, size_t count)
{
    bms_soc_report_t report;
    uint8_t i;

    if (words == NULL || count < BMS_RO_D000_WORDS || !app->has_sample) {
        return false;
    }

    memset(words, 0, count * sizeof(words[0]));
    for (i = 0u; i < BMS_MAX_CELLS; ++i) {
        words[i] = app->last_sample.vcell[i];
    }

    report = bms_app_report(app);
    words[32] = app->last_sample.vcell_max_mv;
    words[33] = app->last_sample.vcell_min_mv;
    words[34] = 0u;
    words[35] = 0u;
    words[36] = app->last_sample.vcell_delta_mv;
    words[37] = clamp_word((uint32_t)app->last_sample.pack_mv / 10u);
    words[48] = app->last_sample.temp_max_c;
    words[49] = app->last_sample.temp_min_c;
    words[50] = app->last_sample.ichg_a10;
    words[51] = app->last_sample.idsg_a10;
    words[52] = report.display_soc;
    words[53] = report.soh;
    words[54] = report.capacity_now_ah100;
    words[55] = report.capacity_full_ah100;
    words[56] = report.capacity_factory_ah100;
    words[57] = report.cycle_count;
    words[58] = (uint16_t)(app->protection.active_faults & 0xFFFFu);
    words[59] = (uint16_t)((app->protection.active_faults >> 16) & 0xFFFFu);
    return true;
}

bool bms_comm_read_d300(const bms_app_t *app, uint16_t *words, size_t count)
{
    bms_soc_report_t report;

    if (words == NULL || count < BMS_RO_D300_WORDS) {
        return false;
    }

    memset(words, 0, count * sizeof(words[0]));
    words[0] = 0u;
    words[2] = 200u;
    words[3] = 5u;
    words[4] = 300u;
    words[15] = BMS_SOC_TEST_UNSUPPORTED;

    if (app != NULL) {
        report = bms_app_report(app);
        words[12] = report.display_soc;
        words[13] = report.soh;
        words[14] = report.capacity_now_ah100;
    }
    return true;
}
