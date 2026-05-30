/* 自动生成，请勿手动修改。源数据: data/examples/param_table.csv */

#ifndef ELT_PARAM_TABLE_H
#define ELT_PARAM_TABLE_H

#include <stdint.h>

typedef enum {
    PARAM_ID_CELL_OVP_MV = 0,
    PARAM_ID_CELL_UVP_MV = 1,
    PARAM_ID_RATED_CAPACITY_MAH = 2,
    PARAM_ID_DEVICE_MODBUS_ADDR = 3,
    PARAM_ID_COUNT = 4
} param_id_t;

typedef struct {
    param_id_t id;
    uint16_t modbus_addr;
    uint8_t reg_count;
    int32_t min_value;
    int32_t max_value;
    int32_t default_value;
    const char *data_type;
    const char *access;
    const char *save_policy;
    const char *name;
} param_meta_t;

extern const param_meta_t g_param_table[PARAM_ID_COUNT];
extern const int32_t g_param_defaults[PARAM_ID_COUNT];

#endif
