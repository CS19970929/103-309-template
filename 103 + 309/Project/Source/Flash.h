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

/* reserved[0..5] hold current calibration; [6..14] are private update tags.
 * Do not reorder groups: their positions are part of the stored format. */
#define STORAGE_PARAM_VERSION_FIRST 6U
/* Private fields fit the existing 24-word reserved area; record size is unchanged. */
#define STORAGE_BAUD_MODE_INDEX 15U
#define STORAGE_BATTERY_NUMBER_INDEX 16U
#define STORAGE_BATTERY_NUMBER_TAG_INDEX 22U
#define STORAGE_BATTERY_NUMBER_TAG 0x424EU
/* Existing private words link a committed cycle command to its SOC snapshot. */
#define STORAGE_SOC_CYCLE_GENERATION_INDEX 23U
#define STORAGE_SOC_CYCLE_GENERATION_NONE 0xFFFFU
#define STORAGE_SOC_CYCLE_PARAMETER_INDEX 25U
#define STORAGE_SOC_CYCLE_SNAPSHOT_INDEX 0U
/* SOC V2 keeps its size. Only this tag pair enables smooth-calibration fields. */
#define STORAGE_SOC_SMOOTH_TAG 0x534DU
#define STORAGE_SOC_LOW_ENTRY_LO_INDEX 1U
#define STORAGE_SOC_LOW_ENTRY_HI_INDEX 2U
#define STORAGE_SOC_SMOOTH_INV_INDEX 3U
#define BATTERY_NUMBER_LENGTH 11U
enum STORAGE_PARAM_GROUP {
    STORAGE_PARAM_AFE, STORAGE_PARAM_PROTECT, STORAGE_PARAM_SOC,
    STORAGE_PARAM_OTHER, STORAGE_PARAM_CALIB, STORAGE_PARAM_HEAT_COOL,
    STORAGE_PARAM_SOC_STATE, STORAGE_PARAM_AGING_STATE, STORAGE_PARAM_LOG,
    STORAGE_PARAM_COUNT
};
#define STORAGE_PARAM_RESET_ALL   1U
#define STORAGE_PARAM_RESET_SOC   2U
#define STORAGE_PARAM_RESET_AGING 4U
#define STORAGE_PARAM_RESET_LOG   8U
#define STORAGE_PARAM_RESET_MASK 15U


/* Factory aging state belongs to the shared state journal. */
#define FLASH_FACTORY_AGING_DONE_VALUE   ((UINT16)0xA93D)
#define FLASH_FACTORY_AGING_STATE_RUNNING ((UINT16)0xA931)
#define FLASH_FACTORY_AGING_STATE_STOPPED ((UINT16)0xA930)
#define FLASH_FACTORY_AGING_STATE_DONE   FLASH_FACTORY_AGING_DONE_VALUE

#define FLASH_NORMAL_SLEEP_VALUE         ((UINT16)0x1234)
#define FLASH_DEEP_SLEEP_VALUE           ((UINT16)0x1235)
#define FLASH_HICCUP_SLEEP_VALUE         ((UINT16)0x1236)
#define FLASH_SLEEP_CHARGER_WAKE_VALUE   ((UINT16)0x1237)
#define FLASH_UNDERVOLTAGE_SLEEP_VALUE   ((UINT16)0x1238)
#define FLASH_SLEEP_RESET_VALUE          ((UINT16)0xFFFF)

#define BOOT_FLAG_RESET_VALUE            FLASH_SLEEP_RESET_VALUE

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
    UINT16 initializing; /* Durable pending STORAGE_PARAM_RESET_* bits. */
} STORAGE_FLASH_CONFIG_DATA;

UINT8 StorageFlash_LoadConfig(STORAGE_FLASH_CONFIG_DATA *data);
UINT8 StorageFlash_SaveConfig(const STORAGE_FLASH_CONFIG_DATA *data);
UINT16 StorageFlash_GetBaudMode(void);
UINT8 StorageFlash_SetBaudMode(UINT16 mode);
UINT8 StorageFlash_ReadBatteryNumber(UINT8 number[BATTERY_NUMBER_LENGTH]);
UINT8 StorageFlash_WriteBatteryNumber(const UINT8 number[BATTERY_NUMBER_LENGTH]);
UINT8 StorageFlash_CommitParameterUpdate(STORAGE_FLASH_CONFIG_DATA *config);
UINT8 StorageFlash_AppendLog(UINT8 event, UINT8 delta);
UINT8 StorageFlash_ClearLog(void);
#define FLASH_STATISTICS_COUNT 11U
void StorageFlash_InitStatistics(UINT32 reset_cause, UINT16 sleep_flag);
void StorageFlash_UpdateFaultStatistics(UINT16 active_faults);
UINT8 StorageFlash_ReadStatistics(UINT32 counts[FLASH_STATISTICS_COUNT]);
UINT8 StorageFlash_ClearFaultStatistics(void);
void StorageFlash_StatisticsTask(void);
void StorageFlash_FlushStatistics(void);

UINT16 FlashReadOneHalfWord(UINT32 faddr);
UINT8 AppUpgrade_RequestIap(void);

UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data);
UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data);
UINT8 StorageFlash_LoadAfeData(UINT16 *values, UINT16 word_count);
UINT8 StorageFlash_SaveAfeData(const UINT16 *values, UINT16 word_count);
UINT8 StorageFlash_LoadRwParamData(STORAGE_FLASH_RW_PARAM_DATA *data);
UINT8 StorageFlash_SaveRwParamData(const STORAGE_FLASH_RW_PARAM_DATA *data, UINT8 cycle_count_written);
UINT16 StorageFlash_GetSocCycleGeneration(void);
UINT8 StorageFlash_ResumeSocCycleWrite(const STORAGE_FLASH_CONFIG_DATA *config);
UINT8 StorageFlash_LoadLogData(UINT8 *point, UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2]);
UINT8 StorageFlash_LoadFactoryAgingData(STORAGE_FLASH_FACTORY_AGING_DATA *data);
UINT8 StorageFlash_SaveFactoryAgingData(const STORAGE_FLASH_FACTORY_AGING_DATA *data);
UINT8 StorageFlash_IsBusy(void);

void StorageFlash_PrintBootCheck(void);
void App_FlashUpdate(void);
void APP_To_IAP_Jump(void);
void InitAreaSelect(void);

#endif
