/* 自动生成，请勿手动修改。源数据: data/param_tables/example_bms_params.csv */

#include "param_table.h"

const param_meta_t g_param_table[PARAM_ID_COUNT] = {
    {
        PARAM_ID_CELL_OVP_MV,
        0x2100U,
        1U,
        PARAM_TYPE_U16,
        3000LL,
        4500LL,
        4200LL,
        PARAM_ACCESS_RW,
        PARAM_SAVE_FLASH,
        "protection",
        "Cell over voltage threshold",
        "cell_ovp_mv",
        "1",
        "mV",
        "单体过压保护阈值"
    },
    {
        PARAM_ID_CELL_UVP_MV,
        0x2101U,
        1U,
        PARAM_TYPE_U16,
        1800LL,
        3300LL,
        2500LL,
        PARAM_ACCESS_RW,
        PARAM_SAVE_FLASH,
        "protection",
        "Cell under voltage threshold",
        "cell_uvp_mv",
        "1",
        "mV",
        "单体欠压保护阈值"
    },
    {
        PARAM_ID_CHG_OCP_DEC_A,
        0x2102U,
        1U,
        PARAM_TYPE_U16,
        0LL,
        2000LL,
        500LL,
        PARAM_ACCESS_RW,
        PARAM_SAVE_FLASH,
        "protection",
        "Charge over current threshold",
        "chg_ocp_dec_a",
        "0.1",
        "A",
        "充电过流保护阈值"
    },
    {
        PARAM_ID_DSG_OCP_DEC_A,
        0x2103U,
        1U,
        PARAM_TYPE_U16,
        0LL,
        3000LL,
        1000LL,
        PARAM_ACCESS_RW,
        PARAM_SAVE_FLASH,
        "protection",
        "Discharge over current threshold",
        "dsg_ocp_dec_a",
        "0.1",
        "A",
        "放电过流保护阈值"
    },
    {
        PARAM_ID_RATED_CAPACITY_MAH,
        0x2200U,
        2U,
        PARAM_TYPE_U32,
        1000LL,
        300000LL,
        100000LL,
        PARAM_ACCESS_RW,
        PARAM_SAVE_FLASH,
        "soc",
        "Rated capacity",
        "rated_capacity_mah",
        "1",
        "mAh",
        "电池包额定容量"
    },
    {
        PARAM_ID_SOC_LOW_HOLD_PCT,
        0x2202U,
        1U,
        PARAM_TYPE_U8,
        0LL,
        100LL,
        5LL,
        PARAM_ACCESS_RW,
        PARAM_SAVE_FLASH,
        "soc",
        "SOC low display hold threshold",
        "soc_low_hold_pct",
        "1",
        "%",
        "SOC 低电量显示保持阈值"
    },
    {
        PARAM_ID_SOC_FULL_ANCHOR_MV,
        0x2203U,
        1U,
        PARAM_TYPE_U16,
        3300LL,
        4500LL,
        4100LL,
        PARAM_ACCESS_RW,
        PARAM_SAVE_FLASH,
        "soc",
        "SOC full voltage anchor",
        "soc_full_anchor_mv",
        "1",
        "mV",
        "SOC 满电电压锚点"
    },
    {
        PARAM_ID_BALANCE_ENABLE,
        0x2300U,
        1U,
        PARAM_TYPE_BOOL,
        0LL,
        1LL,
        1LL,
        PARAM_ACCESS_RW,
        PARAM_SAVE_FLASH,
        "balance",
        "Balance enable",
        "balance_enable",
        "1",
        "enable",
        "均衡功能使能开关"
    },
    {
        PARAM_ID_BALANCE_START_MV,
        0x2301U,
        1U,
        PARAM_TYPE_U16,
        3000LL,
        4300LL,
        3600LL,
        PARAM_ACCESS_RW,
        PARAM_SAVE_FLASH,
        "balance",
        "Balance start voltage",
        "balance_start_mv",
        "1",
        "mV",
        "均衡启动单体电压阈值"
    },
    {
        PARAM_ID_DEVICE_MODBUS_ADDR,
        0x2400U,
        1U,
        PARAM_TYPE_U8,
        1LL,
        247LL,
        1LL,
        PARAM_ACCESS_RW,
        PARAM_SAVE_FLASH,
        "system",
        "Device Modbus address",
        "device_modbus_addr",
        "1",
        "addr",
        "设备 Modbus 从站地址"
    },
    {
        PARAM_ID_AGING_DURATION_HOURS,
        0x2401U,
        1U,
        PARAM_TYPE_U16,
        0LL,
        2000LL,
        72LL,
        PARAM_ACCESS_RW,
        PARAM_SAVE_FACTORY,
        "system",
        "Aging duration",
        "aging_duration_hours",
        "1",
        "h",
        "出厂老化目标时长"
    },
    {
        PARAM_ID_PACK_VOLTAGE_MV,
        0xD000U,
        1U,
        PARAM_TYPE_U16,
        0LL,
        65535LL,
        0LL,
        PARAM_ACCESS_RO,
        PARAM_SAVE_NONE,
        "runtime",
        "Pack voltage monitor",
        "pack_voltage_mv",
        "1",
        "mV",
        "实时电池包总压只读值"
    },
};

const int64_t g_param_defaults[PARAM_ID_COUNT] = {
    4200LL, /* PARAM_ID_CELL_OVP_MV */
    2500LL, /* PARAM_ID_CELL_UVP_MV */
    500LL, /* PARAM_ID_CHG_OCP_DEC_A */
    1000LL, /* PARAM_ID_DSG_OCP_DEC_A */
    100000LL, /* PARAM_ID_RATED_CAPACITY_MAH */
    5LL, /* PARAM_ID_SOC_LOW_HOLD_PCT */
    4100LL, /* PARAM_ID_SOC_FULL_ANCHOR_MV */
    1LL, /* PARAM_ID_BALANCE_ENABLE */
    3600LL, /* PARAM_ID_BALANCE_START_MV */
    1LL, /* PARAM_ID_DEVICE_MODBUS_ADDR */
    72LL, /* PARAM_ID_AGING_DURATION_HOURS */
    0LL, /* PARAM_ID_PACK_VOLTAGE_MV */
};
