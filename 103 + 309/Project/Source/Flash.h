#ifndef PROJECT_STORAGE_FLASH_H
#define PROJECT_STORAGE_FLASH_H

#define FLASH_ADDR_IAP_START             0x08000000
#define FLASH_ADDR_APP_START             0x08004800

/*
 * STM32F103C8 project storage contract.
 *
 * This product intentionally uses the 128KB physical Flash available on the
 * selected C8 devices. The firmware target is STM32F10X_MD, so the erase page
 * is 1KB. APP must never cross FLASH_ADDR_STORAGE_START.
 */
#if !defined(STM32F10X_MD)
#error "STM32F103C8 persistent layout requires STM32F10X_MD 1KB erase pages"
#endif

#define FLASH_STORAGE_PAGE_SIZE           ((UINT32)0x00000400)
#define FLASH_STORAGE_SLOT_SIZE            FLASH_STORAGE_PAGE_SIZE
#define FLASH_STORAGE_RECORD_ALIGNMENT     ((UINT16)4U)
#define FLASH_ADDR_STORAGE_START           ((UINT32)0x0801E000)
#define FLASH_ADDR_STORAGE_END             ((UINT32)0x08020000)
#define FLASH_ADDR_APP_END                 FLASH_ADDR_STORAGE_START
#define FLASH_APP_MAX_SIZE                 (FLASH_ADDR_APP_END - FLASH_ADDR_APP_START)

/* Flash addresses describe storage objects only; parameter categories do not
 * own Flash pages. All configurable BMS parameters share CONFIG A/B. */
#define FLASH_ADDR_STORAGE_CONFIG_SLOT_A   ((UINT32)0x0801E000)
#define FLASH_ADDR_STORAGE_CONFIG_SLOT_B   ((UINT32)0x0801E400)
#define FLASH_ADDR_STORAGE_SOC_SLOT_A      ((UINT32)0x0801E800)
#define FLASH_ADDR_STORAGE_SOC_SLOT_B      ((UINT32)0x0801EC00)
#define FLASH_ADDR_STORAGE_LOG_SLOT_A      ((UINT32)0x0801F000)
#define FLASH_ADDR_STORAGE_LOG_DELTA_A     ((UINT32)0x0801F400)
#define FLASH_ADDR_STORAGE_LOG_SLOT_B      ((UINT32)0x0801F800)
#define FLASH_ADDR_STORAGE_LOG_DELTA_B     ((UINT32)0x0801FC00)

#define BMS_CONFIG_AFE_WORD_COUNT          ((UINT16)24U)
#define BMS_CONFIG_PROTECT_WORD_COUNT      ((UINT16)65U)
#define BMS_CONFIG_CALIB_WORD_COUNT        ((UINT16)47U)
#define BMS_CONFIG_OTHER_WORD_COUNT        ((UINT16)32U)
#define BMS_CONFIG_RESERVED_WORD_COUNT     ((UINT16)24U)
#define FLASH_STORAGE_LOG_RECORD_COUNT     ((UINT16)100U)

#define FLASH_STORAGE_SOC_DATA_VERSION_CURRENT ((UINT16)0x0003U)
#define FLASH_STORAGE_CONFIG_FORMAT_VERSION    ((UINT16)0x0002U)

#define FLASH_309_RTC_RTC_VALUE          ((UINT16)0x1222)
#define FLASH_309_RTC_NORMAL_VALUE       ((UINT16)0x2333)
#define FLASH_309_NORMAL_NORMAL_VALUE    ((UINT16)0xFFFF)
#define FLASH_TO_IAP_VALUE               ((UINT16)0x00AB)
#define FLASH_TO_APP_VALUE               ((UINT16)0xFFFF)
#define FLASH_UPGRADE_PARAM_FLAG_RESET   ((UINT16)0xFFFF)
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

/* One atomic persistent configuration image. Runtime/protocol code may still
 * distinguish AFE, protection, calibration and Other parameter groups, but
 * Flash sees exactly one CONFIG payload. */
typedef struct
{
	UINT16 u16FormatVersion;
	UINT16 u16AppliedPolicyVersion;
	UINT16 afe[BMS_CONFIG_AFE_WORD_COUNT];
	UINT16 protect[BMS_CONFIG_PROTECT_WORD_COUNT];
	UINT16 calibK[BMS_CONFIG_CALIB_WORD_COUNT];
	INT16 calibB[BMS_CONFIG_CALIB_WORD_COUNT];
	UINT16 other[BMS_CONFIG_OTHER_WORD_COUNT];
	UINT16 reserved[BMS_CONFIG_RESERVED_WORD_COUNT];
} BMS_CONFIG;

FLASH_Status FlashWriteOneHalfWord(uint32_t StartAddr, uint16_t Buffer);
UINT16 FlashReadOneHalfWord(UINT32 faddr);
UINT8 AppUpgrade_RequestIap(void);

UINT8 StorageFlash_LoadConfigData(BMS_CONFIG *data);
UINT8 StorageFlash_SaveConfigData(const BMS_CONFIG *data);
UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data);
UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data);
UINT8 StorageFlash_LoadLogData(UINT8 *point, UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2]);
UINT8 StorageFlash_SaveLogData(UINT8 point, const UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2]);

/* Bounded raw operations exist only for storage internals such as Log Delta. */
UINT8 StorageFlash_EraseStoragePage(UINT32 page_addr);
UINT8 StorageFlash_ProgramStorageBytes(UINT32 addr, const UINT8 *data, UINT16 length);
UINT8 StorageFlash_IsBusy(void);

void StorageFlash_PrintBootCheck(void);
void App_FlashUpdate(void);
void APP_To_IAP_Jump(void);
void InitAreaSelect(void);

#endif
