/* 自动生成，请勿手动修改。源数据: data/examples/param_table.csv */

#include "param_table.h"

const param_meta_t g_param_table[PARAM_ID_COUNT] = {
    { PARAM_ID_CELL_OVP_MV, 0x2100U, 1U, 3000, 4500, 4200, "u16", "rw", "flash", "Cell OVP" },
    { PARAM_ID_CELL_UVP_MV, 0x2101U, 1U, 1800, 3300, 2500, "u16", "rw", "flash", "Cell UVP" },
    { PARAM_ID_RATED_CAPACITY_MAH, 0x2200U, 2U, 1000, 300000, 100000, "u32", "rw", "flash", "Rated capacity" },
    { PARAM_ID_DEVICE_MODBUS_ADDR, 0x2400U, 1U, 1, 247, 1, "u8", "rw", "flash", "Device address" },
};

const int32_t g_param_defaults[PARAM_ID_COUNT] = {
    4200, /* PARAM_ID_CELL_OVP_MV */
    2500, /* PARAM_ID_CELL_UVP_MV */
    100000, /* PARAM_ID_RATED_CAPACITY_MAH */
    1, /* PARAM_ID_DEVICE_MODBUS_ADDR */
};
