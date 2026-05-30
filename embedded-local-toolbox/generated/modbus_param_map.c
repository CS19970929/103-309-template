/* 自动生成，请勿手动修改。源数据: data/examples/param_table.csv */

#include "param_table.h"

typedef struct { uint16_t addr; param_id_t id; uint8_t reg_count; } modbus_param_map_t;

const modbus_param_map_t g_modbus_param_map[PARAM_ID_COUNT] = {
    { 0x2100U, PARAM_ID_CELL_OVP_MV, 1U },
    { 0x2101U, PARAM_ID_CELL_UVP_MV, 1U },
    { 0x2200U, PARAM_ID_RATED_CAPACITY_MAH, 2U },
    { 0x2400U, PARAM_ID_DEVICE_MODBUS_ADDR, 1U },
};
