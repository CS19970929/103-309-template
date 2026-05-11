#ifndef BMS_TYPES_H
#define BMS_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define BMS_CELL_COUNT_MAX 16U

typedef enum
{
    BMS_POWER_RUN = 0,
    BMS_POWER_RTC_IDLE,
    BMS_POWER_DEEP_PENDING,
    BMS_POWER_STOP
} BmsPowerState;

typedef enum
{
    BMS_SLEEP_NONE = 0,
    BMS_SLEEP_RTC_IDLE,
    BMS_SLEEP_DEEP
} BmsSleepMode;

typedef enum
{
    BMS_WAKE_NONE = 0,
    BMS_WAKE_RTC,
    BMS_WAKE_KEY,
    BMS_WAKE_CHARGER,
    BMS_WAKE_COMM,
    BMS_WAKE_RESET
} BmsWakeReason;

enum
{
    BMS_FAULT_CELL_OV = 0x00000001UL,
    BMS_FAULT_CELL_UV = 0x00000002UL,
    BMS_FAULT_CHG_OT = 0x00000004UL,
    BMS_FAULT_DSG_OT = 0x00000008UL,
    BMS_FAULT_AFE = 0x00000010UL
};

typedef struct
{
    uint8_t cell_count;
    uint16_t cell_mv[BMS_CELL_COUNT_MAX];
    uint16_t vcell_min_mv;
    uint16_t vcell_max_mv;
    uint16_t pack_mv;
    int32_t current_ma;
    int16_t temp_c_x10;
    bool charger_present;
    bool load_present;
    uint32_t uptime_ms;
} BmsSample;

typedef struct
{
    uint8_t cell_count;
    uint16_t capacity_mah;
    uint16_t soc_full_mv;
    uint16_t soc_empty_mv;
    uint16_t sleep_idle_seconds;
    uint16_t rtc_period_seconds;
    uint16_t current_idle_ma;
} BmsConfig;

typedef struct
{
    int32_t remain_mah;
    uint16_t soc_permille;
    uint8_t display_soc_percent;
    uint8_t soh_percent;
    uint16_t cycle_count;
    uint32_t faults;
    bool charge_allowed;
    bool discharge_allowed;
    BmsPowerState power_state;
    BmsWakeReason wake_reason;
} BmsState;

typedef struct
{
    BmsConfig config;
    BmsSample sample;
    BmsState state;
} BmsSnapshot;

#endif
