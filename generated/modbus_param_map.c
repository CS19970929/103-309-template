/* 自动生成，请勿手动修改。源数据: data/param_tables/example_bms_params.csv */

#include "param_table.h"

const param_modbus_map_t g_modbus_param_map[PARAM_ID_COUNT] = {
    { 0x2100U, PARAM_ID_CELL_OVP_MV, 1U },
    { 0x2101U, PARAM_ID_CELL_UVP_MV, 1U },
    { 0x2102U, PARAM_ID_CHG_OCP_DEC_A, 1U },
    { 0x2103U, PARAM_ID_DSG_OCP_DEC_A, 1U },
    { 0x2200U, PARAM_ID_RATED_CAPACITY_MAH, 2U },
    { 0x2202U, PARAM_ID_SOC_LOW_HOLD_PCT, 1U },
    { 0x2203U, PARAM_ID_SOC_FULL_ANCHOR_MV, 1U },
    { 0x2300U, PARAM_ID_BALANCE_ENABLE, 1U },
    { 0x2301U, PARAM_ID_BALANCE_START_MV, 1U },
    { 0x2400U, PARAM_ID_DEVICE_MODBUS_ADDR, 1U },
    { 0x2401U, PARAM_ID_AGING_DURATION_HOURS, 1U },
    { 0xD000U, PARAM_ID_PACK_VOLTAGE_MV, 1U },
};

const uint16_t g_modbus_param_map_count = (uint16_t)PARAM_ID_COUNT;
