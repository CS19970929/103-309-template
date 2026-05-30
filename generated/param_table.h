/* 自动生成，请勿手动修改。源数据: data/param_tables/example_bms_params.csv */

#ifndef GENERATED_PARAM_TABLE_H
#define GENERATED_PARAM_TABLE_H

#include <stdint.h>

typedef enum {
    PARAM_ID_CELL_OVP_MV = 0,
    PARAM_ID_CELL_UVP_MV = 1,
    PARAM_ID_CHG_OCP_DEC_A = 2,
    PARAM_ID_DSG_OCP_DEC_A = 3,
    PARAM_ID_RATED_CAPACITY_MAH = 4,
    PARAM_ID_SOC_LOW_HOLD_PCT = 5,
    PARAM_ID_SOC_FULL_ANCHOR_MV = 6,
    PARAM_ID_BALANCE_ENABLE = 7,
    PARAM_ID_BALANCE_START_MV = 8,
    PARAM_ID_DEVICE_MODBUS_ADDR = 9,
    PARAM_ID_AGING_DURATION_HOURS = 10,
    PARAM_ID_PACK_VOLTAGE_MV = 11,
    PARAM_ID_COUNT = 12
} param_id_t;

typedef enum {
    PARAM_TYPE_BOOL = 0,
    PARAM_TYPE_S16 = 1,
    PARAM_TYPE_S32 = 2,
    PARAM_TYPE_S8 = 3,
    PARAM_TYPE_U16 = 4,
    PARAM_TYPE_U32 = 5,
    PARAM_TYPE_U8 = 6
} param_data_type_t;

typedef enum {
    PARAM_ACCESS_RO = 0,
    PARAM_ACCESS_RW = 1,
    PARAM_ACCESS_WO = 2
} param_access_t;

typedef enum {
    PARAM_SAVE_EEPROM = 0,
    PARAM_SAVE_FACTORY = 1,
    PARAM_SAVE_FLASH = 2,
    PARAM_SAVE_NONE = 3,
    PARAM_SAVE_NVM = 4,
    PARAM_SAVE_RUNTIME = 5
} param_save_policy_t;

typedef struct {
    param_id_t id;
    uint16_t modbus_addr;
    uint8_t reg_count;
    param_data_type_t data_type;
    int64_t min_value;
    int64_t max_value;
    int64_t default_value;
    param_access_t access;
    param_save_policy_t save_policy;
    const char *group;
    const char *name;
    const char *c_name;
    const char *scale;
    const char *unit;
    const char *description;
} param_meta_t;

typedef struct {
    uint16_t modbus_addr;
    param_id_t id;
    uint8_t reg_count;
} param_modbus_map_t;

extern const param_meta_t g_param_table[PARAM_ID_COUNT];
extern const int64_t g_param_defaults[PARAM_ID_COUNT];
extern const param_modbus_map_t g_modbus_param_map[PARAM_ID_COUNT];
extern const uint16_t g_modbus_param_map_count;

#endif /* GENERATED_PARAM_TABLE_H */
