#ifndef PROJECT_STORAGE_FLASH_H
#define PROJECT_STORAGE_FLASH_H

#define FLASH_ADDR_IAP_START             0x08000000
#define FLASH_ADDR_APP_START             0x08004800

/* Confirmed STM32F103CB: 128 KiB Flash, 1 KiB erase page. */
#define FLASH_STORAGE_PAGE_SIZE ((UINT32)0x400U)
#define FLASH_PARAMETER_RESET_BEGIN ((UINT32)0x0801C000U)

/* Counts shared by runtime parameters and host register windows. */
#define E2P_PARA_NUM_PROTECT 65
#define E2P_PARA_NUM_RTC 12
#define E2P_PARA_NUM_OTHER_ELEMENT1 32

#define FLASH_STORAGE_AFE_WORD_COUNT     ((UINT16)24)
#define FLASH_STORAGE_LOG_RECORD_COUNT   ((UINT16)100)
#define FLASH_STORAGE_SOC_DATA_VERSION ((UINT16)0x0002)

#define STORAGE_SOC_CYCLE_GENERATION_NONE 0xFFFFU
#define STORAGE_SOC_CYCLE_PARAMETER_INDEX 25U
#define STORAGE_SOC_CYCLE_SNAPSHOT_INDEX 0U
/* V2 layout is fixed. The current tag omits derived percentages/inverse;
 * the old tag pair is read only for existing smooth-calibration snapshots. */
#define STORAGE_SOC_STATE_TAG 0x5343U
#define STORAGE_SOC_LOW_ENTRY_LO_INDEX 1U
#define STORAGE_SOC_LOW_ENTRY_HI_INDEX 2U
#define BATTERY_NUMBER_LENGTH 11U
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
	UINT16 u16SocNow;           /* Reserved: always zero in current snapshots. */
	UINT16 u16DsgSocInt;        /* Reserved: always zero in current snapshots. */
	UINT16 u16MaxErrorPercent;  /* Full-calibration voltage, mV. */
	UINT32 u32CycleTimes;       /* Equivalent discharge cycles * 100. */
	UINT32 u32CapNow;           /* Remaining capacity, 0.1 A*s. */
	UINT32 u32CapFull;          /* Capacity basis for proportional restore. */
	UINT32 u32LearnPassedAs10;  /* Discharge below the next 0.01 cycle. */
	UINT16 u16LearnAnchorSoc;   /* Retired manual-target slot; new snapshots use 0xFFFF. */
	UINT16 u16LearnState;       /* SOC state tag, not a learning state. */
	UINT16 u16Flags;            /* Confirmed full, charge evidence; old empty tag is retired. */
	UINT16 u16Reserved[4];      /* Cycle generation, low reference lo/hi, unused. */
} STORAGE_FLASH_SOC_DATA;

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

UINT8 StorageFlash_InitParameters(void);
UINT8 StorageFlash_SaveParameters(UINT8 cycle_count_written);
UINT16 StorageFlash_GetBaudMode(void);
UINT8 StorageFlash_SetBaudMode(UINT16 mode);
UINT8 StorageFlash_ReadSerialNumber(UINT8 *serial, UINT16 length);
UINT8 StorageFlash_WriteSerialNumber(const UINT8 *serial, UINT16 length);
UINT8 StorageFlash_ReadBatteryNumber(UINT8 number[BATTERY_NUMBER_LENGTH]);
UINT8 StorageFlash_WriteBatteryNumber(const UINT8 number[BATTERY_NUMBER_LENGTH]);
UINT8 StorageFlash_AppendLog(UINT8 event, UINT8 delta);
UINT8 StorageFlash_ClearLog(void);
#define FLASH_STATISTICS_COUNT 11U
void StorageFlash_InitStatistics(UINT32 reset_cause, UINT16 sleep_flag);
void StorageFlash_UpdateFaultStatistics(UINT16 active_faults);
UINT8 StorageFlash_ReadStatistics(UINT32 counts[FLASH_STATISTICS_COUNT]);
UINT8 StorageFlash_ClearFaultStatistics(void);
void StorageFlash_StatisticsTask(void);
void StorageFlash_FlushStatistics(void);

UINT8 AppUpgrade_RequestIap(void);

UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data);
UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data);
UINT8 StorageFlash_SaveAfeData(const UINT16 *values, UINT16 word_count);
UINT16 StorageFlash_GetSocCycleGeneration(void);
UINT8 StorageFlash_LoadLogData(UINT8 *point, UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2]);

void App_FlashUpdate(void);

#endif
