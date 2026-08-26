#ifndef PROJECT_STORAGE_FLASH_H
#define PROJECT_STORAGE_FLASH_H

#define FLASH_ADDR_IAP_START             0x08000000U
#define FLASH_ADDR_APP_START             0x08004800U

/*
 * STM32F103C8 official 64KB Flash contract.
 *
 * Only 0x08000000..0x0800FFFF is considered usable. The undocumented rear
 * 64KB found on some C8 devices is deliberately not used by APP or storage.
 * STM32F10X_MD uses 1KB erase pages.
 *
 * Layout:
 *   IAP          : 0x08000000..0x080047FF  (18KB)
 *   APP          : 0x08004800..0x0800E7FF  (40KB max)
 *   CONFIG A/B   : 0x0800E800..0x0800EFFF  (2KB)
 *   SOC A/B      : 0x0800F000..0x0800F7FF  (2KB)
 *   LOG A/B      : 0x0800F800..0x0800FFFF  (2KB)
 *
 * Persistent storage therefore occupies the last six official 1KB pages.
 * APP/linker configuration must never cross FLASH_ADDR_STORAGE_START.
 */
#if !defined(STM32F10X_MD)
#error "STM32F103C8 persistent layout requires STM32F10X_MD 1KB erase pages"
#endif

/* Keep address/page constants as plain integer constant expressions: several
 * guards below are evaluated by the ARMCC5 preprocessor, where C casts are not
 * valid inside #if expressions. */
#define FLASH_DEVICE_OFFICIAL_SIZE        0x00010000U
#define FLASH_ADDR_DEVICE_END             0x08010000U
#define FLASH_STORAGE_PAGE_SIZE           0x00000400U
#define FLASH_STORAGE_SLOT_SIZE            FLASH_STORAGE_PAGE_SIZE
#define FLASH_STORAGE_RECORD_ALIGNMENT     ((UINT16)4U)
#define FLASH_ADDR_STORAGE_START           0x0800E800U
#define FLASH_ADDR_STORAGE_END             FLASH_ADDR_DEVICE_END
#define FLASH_ADDR_APP_END                 FLASH_ADDR_STORAGE_START
#define FLASH_APP_MAX_SIZE                 (FLASH_ADDR_APP_END - FLASH_ADDR_APP_START)

/* Flash addresses describe storage objects only; parameter categories do not
 * own Flash pages. All configurable BMS parameters share CONFIG A/B. */
#define FLASH_ADDR_STORAGE_CONFIG_SLOT_A   0x0800E800U
#define FLASH_ADDR_STORAGE_CONFIG_SLOT_B   0x0800EC00U
#define FLASH_ADDR_STORAGE_SOC_SLOT_A      0x0800F000U
#define FLASH_ADDR_STORAGE_SOC_SLOT_B      0x0800F400U
#define FLASH_ADDR_STORAGE_LOG_SLOT_A      0x0800F800U
#define FLASH_ADDR_STORAGE_LOG_SLOT_B      0x0800FC00U

/* Compile-time geometry guards: changing one address cannot silently move
 * storage outside the official C8 Flash or create page overlap/gaps. */
#if (FLASH_ADDR_STORAGE_START != 0x0800E800U)
#error "Unexpected persistent storage start"
#endif
#if (FLASH_ADDR_STORAGE_END != 0x08010000U)
#error "Persistent storage must end at the official 64KB boundary"
#endif
#if ((FLASH_ADDR_STORAGE_END - FLASH_ADDR_STORAGE_START) != (6U * 0x400U))
#error "Persistent storage must occupy exactly six 1KB pages"
#endif
#if ((FLASH_ADDR_STORAGE_CONFIG_SLOT_B - FLASH_ADDR_STORAGE_CONFIG_SLOT_A) != 0x400U) || \
    ((FLASH_ADDR_STORAGE_SOC_SLOT_A - FLASH_ADDR_STORAGE_CONFIG_SLOT_B) != 0x400U) || \
    ((FLASH_ADDR_STORAGE_SOC_SLOT_B - FLASH_ADDR_STORAGE_SOC_SLOT_A) != 0x400U) || \
    ((FLASH_ADDR_STORAGE_LOG_SLOT_A - FLASH_ADDR_STORAGE_SOC_SLOT_B) != 0x400U) || \
    ((FLASH_ADDR_STORAGE_LOG_SLOT_B - FLASH_ADDR_STORAGE_LOG_SLOT_A) != 0x400U) || \
    ((FLASH_ADDR_STORAGE_END - FLASH_ADDR_STORAGE_LOG_SLOT_B) != 0x400U)
#error "Persistent storage slots must be contiguous 1KB pages"
#endif
#if ((FLASH_ADDR_APP_START + 0x0000A000U) != FLASH_ADDR_APP_END)
#error "APP region must be exactly 40KB"
#endif

/* These count macros are intentionally plain integer constant expressions.
 * EEPROM.c compares them in #if directives, where C type casts such as
 * ((UINT16)24U) are not valid preprocessor expressions on ARMCC5. */
#define BMS_CONFIG_AFE_WORD_COUNT          24U
#define BMS_CONFIG_PROTECT_WORD_COUNT      65U
#define BMS_CONFIG_CALIB_WORD_COUNT        47U
#define BMS_CONFIG_OTHER_WORD_COUNT        32U
#define BMS_CONFIG_RESERVED_WORD_COUNT     24U
#define FLASH_STORAGE_LOG_RECORD_COUNT     100U

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

/* Shared CRC16/Modbus primitive used by persistent record formats. */
UINT16 StorageFlash_Crc16Update(UINT16 crc, const UINT8 *data, UINT16 length);
UINT16 StorageFlash_Crc16(const UINT8 *data, UINT16 length);

/* Bounded raw operations exist only for storage internals such as log journal. */
UINT8 StorageFlash_EraseStoragePage(UINT32 page_addr);
UINT8 StorageFlash_ProgramStorageBytes(UINT32 addr, const UINT8 *data, UINT16 length);
UINT8 StorageFlash_IsBusy(void);

void StorageFlash_PrintBootCheck(void);
void App_FlashUpdate(void);
void APP_To_IAP_Jump(void);
void InitAreaSelect(void);

#endif
