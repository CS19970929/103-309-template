#ifndef PROJECT_STORAGE_FLASH_H
#define PROJECT_STORAGE_FLASH_H

#define FLASH_ADDR_IAP_START             0x08000000
#define FLASH_ADDR_APP_START             0x08004800

/* Confirmed STM32F103CB: 128 KiB Flash, 1 KiB erase page. */
#define FLASH_STORAGE_PAGE_SIZE ((UINT32)0x400U)
#define FLASH_PARAMETER_RESET_BEGIN ((UINT32)0x0801C000U)

#define FLASH_STORAGE_AFE_WORD_COUNT     ((UINT16)24)
#define FLASH_STORAGE_RW_PARAM_PROTECT_WORD_COUNT   65
#define FLASH_STORAGE_RW_PARAM_OTHER_WORD_COUNT     32
#define FLASH_STORAGE_RW_PARAM_RESERVED_WORD_COUNT 24
#define FLASH_STORAGE_RW_PARAM_HEAT_COOL_WORD_COUNT 24
#define FLASH_STORAGE_LOG_RECORD_COUNT   ((UINT16)100)
#define FLASH_STORAGE_SOC_DATA_VERSION ((UINT16)0x0002)


/* Factory aging state belongs to the shared state journal. */
#define FLASH_FACTORY_AGING_DONE_VALUE   ((UINT16)0xA93D)
#define FLASH_FACTORY_AGING_STATE_RUNNING ((UINT16)0xA931)
#define FLASH_FACTORY_AGING_STATE_STOPPED ((UINT16)0xA930)
#define FLASH_FACTORY_AGING_STATE_DONE   FLASH_FACTORY_AGING_DONE_VALUE

#define FLASH_NORMAL_SLEEP_VALUE         ((UINT16)0x1234)
#define FLASH_DEEP_SLEEP_VALUE           ((UINT16)0x1235)
#define FLASH_HICCUP_SLEEP_VALUE         ((UINT16)0x1236)
#define FLASH_SLEEP_CHARGER_WAKE_VALUE   ((UINT16)0x1237)
#define FLASH_SLEEP_RESET_VALUE          ((UINT16)0xFFFF)

#define BOOT_FLAG_RESET_VALUE            FLASH_SLEEP_RESET_VALUE
#define STORAGE_FLASH_SOC_API_DECLARED   1

typedef struct
{
	UINT16 u16FormatVersion;
	UINT16 u16SocNow;
	UINT16 u16DsgSocInt;
	UINT16 u16MaxErrorPercent;
	UINT32 u32CycleTimes;
	UINT32 u32CapNow;
	UINT32 u32CapFull;
	UINT32 u32LearnPassedAs10;
	UINT16 u16LearnAnchorSoc;
	UINT16 u16LearnState;
	UINT16 u16Flags;
	UINT16 u16Reserved[4];
} STORAGE_FLASH_SOC_DATA;

typedef struct
{
	UINT16 protect[FLASH_STORAGE_RW_PARAM_PROTECT_WORD_COUNT];
	UINT16 other[FLASH_STORAGE_RW_PARAM_OTHER_WORD_COUNT];
	UINT16 reserved[FLASH_STORAGE_RW_PARAM_RESERVED_WORD_COUNT];
	UINT16 heat_cool[FLASH_STORAGE_RW_PARAM_HEAT_COOL_WORD_COUNT];
} STORAGE_FLASH_RW_PARAM_DATA;

typedef struct
{
	UINT32 u32Elapsed10ms;
	UINT16 u16State;
	UINT16 u16DurationHours;
} STORAGE_FLASH_FACTORY_AGING_DATA;

/* STM32F103CB: eight 1 KiB pages. Statistics are outside parameter resets. */
#define FLASH_STORAGE_BEGIN             ((UINT32)0x0801E000U)
#define FLASH_STORAGE_END               ((UINT32)0x08020000U)
#define FLASH_CONFIG_A                  FLASH_STORAGE_BEGIN
#define FLASH_CONFIG_B                  ((UINT32)0x0801E400U)
#define FLASH_STATE_A                   ((UINT32)0x0801E800U)
#define FLASH_STATE_B                   ((UINT32)0x0801EC00U)
#define FLASH_LOG_A                     ((UINT32)0x0801F000U)
#define FLASH_LOG_B                     ((UINT32)0x0801F400U)
#define FLASH_STATS_A                   ((UINT32)0x0801F800U)
#define FLASH_STATS_B                   ((UINT32)0x0801FC00U)

typedef struct {
    UINT16 afe[FLASH_STORAGE_AFE_WORD_COUNT];
    STORAGE_FLASH_RW_PARAM_DATA rw;
    UINT8 reset_version[32];
    UINT16 initializing;
} STORAGE_FLASH_CONFIG_DATA;

UINT8 StorageFlash_LoadConfig(STORAGE_FLASH_CONFIG_DATA *data);
UINT8 StorageFlash_SaveConfig(const STORAGE_FLASH_CONFIG_DATA *data);
UINT8 StorageFlash_ResetParameters(STORAGE_FLASH_CONFIG_DATA *defaults);
UINT8 StorageFlash_AppendLog(UINT8 event, UINT8 delta);
UINT8 StorageFlash_ClearLog(void);

UINT16 FlashReadOneHalfWord(UINT32 faddr);
UINT8 AppUpgrade_RequestIap(void);

UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data);
UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data);
UINT8 StorageFlash_LoadAfeData(UINT16 *values, UINT16 word_count);
UINT8 StorageFlash_SaveAfeData(const UINT16 *values, UINT16 word_count);
UINT8 StorageFlash_LoadRwParamData(STORAGE_FLASH_RW_PARAM_DATA *data);
UINT8 StorageFlash_SaveRwParamData(const STORAGE_FLASH_RW_PARAM_DATA *data);
UINT8 StorageFlash_LoadLogData(UINT8 *point, UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2]);
UINT8 StorageFlash_LoadFactoryAgingData(STORAGE_FLASH_FACTORY_AGING_DATA *data);
UINT8 StorageFlash_SaveFactoryAgingData(const STORAGE_FLASH_FACTORY_AGING_DATA *data);
UINT8 StorageFlash_IsBusy(void);

void StorageFlash_PrintBootCheck(void);
void App_FlashUpdate(void);
void APP_To_IAP_Jump(void);
void InitAreaSelect(void);

#endif
