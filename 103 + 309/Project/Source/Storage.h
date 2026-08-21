#ifndef PROJECT_STORAGE_H
#define PROJECT_STORAGE_H

#include <stdint.h>

/*
 * Public non-volatile storage API.
 *
 * Business modules should depend on this file only. The current backend is
 * STM32F1 internal Flash, but callers must not depend on physical addresses,
 * page size, A/B slots or journal details.
 */

/* Keep count/version macros preprocessor-safe because some are used by #if. */
#define STORAGE_AFE_WORD_COUNT                 24U
#define STORAGE_RW_PARAM_PROTECT_WORD_COUNT    65U
#define STORAGE_RW_PARAM_OTHER_WORD_COUNT      32U
#define STORAGE_RW_PARAM_RESERVED_WORD_COUNT   24U
#define STORAGE_LOG_RECORD_COUNT               100U
#define STORAGE_SOC_DATA_VERSION_V2            0x0002U

typedef enum
{
    STORAGE_STATE_UNINITIALIZED = 0,
    STORAGE_STATE_READY = 1,
    STORAGE_STATE_READY_UNVERIFIED_CAPACITY = 2,
    STORAGE_STATE_UNSUPPORTED_FLASH = 3
} STORAGE_STATE;

typedef enum
{
    STORAGE_OBJECT_AFE = 0,
    STORAGE_OBJECT_RW_PARAM,
    STORAGE_OBJECT_SOC,
    STORAGE_OBJECT_EVENT_LOG,
    STORAGE_OBJECT_FACTORY_AGING,
    STORAGE_OBJECT_COUNT
} STORAGE_OBJECT;

typedef struct
{
    uint16_t u16FormatVersion;
    uint16_t u16SocNow;
    uint16_t u16DsgSocInt;
    uint16_t u16MaxErrorPercent;
    uint32_t u32CycleTimes;
    uint32_t u32CapNow;
    uint32_t u32CapFull;
    uint32_t u32LearnPassedAs10;
    uint16_t u16LearnAnchorSoc;
    uint16_t u16LearnState;
    uint16_t u16Flags;
    uint16_t u16Reserved[4];
} STORAGE_SOC_DATA;

typedef struct
{
    uint16_t protect[STORAGE_RW_PARAM_PROTECT_WORD_COUNT];
    uint16_t other[STORAGE_RW_PARAM_OTHER_WORD_COUNT];
    uint16_t reserved[STORAGE_RW_PARAM_RESERVED_WORD_COUNT];
} STORAGE_RW_PARAM_DATA;

typedef struct
{
    uint32_t u32Elapsed10ms;
    uint16_t u16State;
    uint16_t u16DurationHours;
} STORAGE_FACTORY_AGING_DATA;

void Storage_Init(void);
uint8_t Storage_IsReady(void);
uint8_t Storage_IsVerifiedCapacity(void);
uint8_t Storage_IsBusy(void);
uint16_t Storage_GetPhysicalFlashKb(void);
STORAGE_STATE Storage_GetState(void);

uint8_t Storage_LoadSocData(STORAGE_SOC_DATA *data);
uint8_t Storage_SaveSocData(const STORAGE_SOC_DATA *data);
uint8_t Storage_LoadAfeData(uint16_t *values, uint16_t word_count);
uint8_t Storage_SaveAfeData(const uint16_t *values, uint16_t word_count);
uint8_t Storage_LoadRwParamData(STORAGE_RW_PARAM_DATA *data);
uint8_t Storage_SaveRwParamData(const STORAGE_RW_PARAM_DATA *data);

/*
 * Event-log API.
 *
 * Storage_LogAppend() is the normal runtime path: one event is appended as a
 * small delta record. Storage_LogLoad() reconstructs the 100-entry ring from
 * the latest A/B snapshot plus valid deltas. Storage_LogClear() atomically
 * advances the base snapshot so stale deltas can never reappear after reset.
 *
 * Storage_LoadLogData()/Storage_SaveLogData() remain as the legacy snapshot
 * compatibility API used by the backend and old code during migration.
 */
uint8_t Storage_LogLoad(uint8_t *point,
                        uint8_t records[STORAGE_LOG_RECORD_COUNT][2]);
uint8_t Storage_LogAppend(uint8_t event, uint8_t delta);
uint8_t Storage_LogClear(void);
uint8_t Storage_LoadLogData(uint8_t *point,
                            uint8_t records[STORAGE_LOG_RECORD_COUNT][2]);
uint8_t Storage_SaveLogData(uint8_t point,
                            const uint8_t records[STORAGE_LOG_RECORD_COUNT][2]);

uint8_t Storage_LoadFactoryAgingData(STORAGE_FACTORY_AGING_DATA *data);
uint8_t Storage_SaveFactoryAgingData(const STORAGE_FACTORY_AGING_DATA *data);

#endif /* PROJECT_STORAGE_H */
