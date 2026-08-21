#ifndef PROJECT_STORAGE_FLASH_H
#define PROJECT_STORAGE_FLASH_H

#include "Storage.h"

/*
 * STM32F1 internal-Flash backend.
 *
 * Keep physical layout and legacy boot/update definitions here. New business
 * code must use Storage.h instead of depending on these addresses directly.
 */

#define FLASH_ADDR_IAP_START                 ((uint32_t)0x08000000U)
#define FLASH_ADDR_APP_START                 ((uint32_t)0x08004800U)
#define FLASH_STORAGE_FLASH_BASE             ((uint32_t)0x08000000U)
#define FLASH_STORAGE_REQUIRED_END           ((uint32_t)0x08020000U)
#define FLASH_STORAGE_REQUIRED_KB            ((uint16_t)128U)

/*
 * STM32F103C8 is officially a 64-KB part. Existing products may deliberately
 * use dies that expose the rear 64 KB. Keep compatibility enabled on dev, but
 * report STORAGE_STATE_READY_UNVERIFIED_CAPACITY so production projects can
 * turn this off explicitly.
 */
#ifndef FLASH_STORAGE_ALLOW_UNVERIFIED_REAR64
#define FLASH_STORAGE_ALLOW_UNVERIFIED_REAR64 1
#endif

#if defined(STM32F10X_MD)
#define FLASH_STORAGE_PAGE_SIZE              ((uint32_t)0x00000400U)
#else
#error "This Flash backend currently targets STM32F1 medium-density 1-KB pages. Port the backend before changing MCU density."
#endif

#define FLASH_STORAGE_SLOT_SIZE              FLASH_STORAGE_PAGE_SIZE
#define FLASH_STORAGE_BASE_ADDR              ((uint32_t)0x0801C000U)
#define FLASH_STORAGE_PAGE_ADDR(index)       (FLASH_STORAGE_BASE_ADDR + ((uint32_t)(index) * FLASH_STORAGE_PAGE_SIZE))

/* 16 reserved pages: 0x0801C000..0x0801FFFF. */
#define FLASH_ADDR_STORAGE_AFE_SLOT_A        FLASH_STORAGE_PAGE_ADDR(0U)
#define FLASH_ADDR_STORAGE_RW_PARAM_SLOT_A   FLASH_STORAGE_PAGE_ADDR(1U)
#define FLASH_ADDR_STORAGE_AFE_SLOT_B        FLASH_STORAGE_PAGE_ADDR(2U)
#define FLASH_ADDR_STORAGE_RW_PARAM_SLOT_B   FLASH_STORAGE_PAGE_ADDR(3U)
#define FLASH_ADDR_STORAGE_LOG_SLOT_A        FLASH_STORAGE_PAGE_ADDR(4U)
#define FLASH_ADDR_STORAGE_LOG_DELTA_A       FLASH_STORAGE_PAGE_ADDR(5U)
#define FLASH_ADDR_STORAGE_LOG_SLOT_B        FLASH_STORAGE_PAGE_ADDR(6U)
#define FLASH_ADDR_STORAGE_LOG_DELTA_B       FLASH_STORAGE_PAGE_ADDR(7U)
#define FLASH_ADDR_STORAGE_SOC_SLOT_A        FLASH_STORAGE_PAGE_ADDR(8U)
#define FLASH_ADDR_STORAGE_FACTORY_SLOT_A    FLASH_STORAGE_PAGE_ADDR(9U)
#define FLASH_ADDR_STORAGE_SOC_SLOT_B        FLASH_STORAGE_PAGE_ADDR(10U)
#define FLASH_ADDR_STORAGE_FACTORY_SLOT_B    FLASH_STORAGE_PAGE_ADDR(11U)
#define FLASH_ADDR_UPGRADE_PARAM_FLAG        FLASH_STORAGE_PAGE_ADDR(12U)
#define FLASH_ADDR_FACTORY_AGING_FLAG        FLASH_STORAGE_PAGE_ADDR(13U)
#define FLASH_ADDR_UPDATE_FLAG               FLASH_STORAGE_PAGE_ADDR(14U)
#define FLASH_ADDR_SLEEP_FLAG                FLASH_STORAGE_PAGE_ADDR(15U)

/* Old reserved names remain source-compatible with the incremental migration. */
#define FLASH_ADDR_STORAGE_RESERVED_5        FLASH_ADDR_STORAGE_LOG_DELTA_A
#define FLASH_ADDR_STORAGE_RESERVED_7        FLASH_ADDR_STORAGE_LOG_DELTA_B
#define FLASH_ADDR_STORAGE_RESERVED_9        FLASH_ADDR_STORAGE_FACTORY_SLOT_A
#define FLASH_ADDR_STORAGE_RESERVED_11       FLASH_ADDR_STORAGE_FACTORY_SLOT_B

/* Legacy aliases retained while old modules are migrated to Storage.h. */
#define FLASH_ADDR_SH367309_VALUE            FLASH_ADDR_STORAGE_SOC_SLOT_A
#define FLASH_ADDR_SH367309_FLAG             FLASH_ADDR_STORAGE_SOC_SLOT_B
#define FLASH_STORAGE_AFE_WORD_COUNT         STORAGE_AFE_WORD_COUNT
#define FLASH_STORAGE_RW_PARAM_PROTECT_WORD_COUNT STORAGE_RW_PARAM_PROTECT_WORD_COUNT
#define FLASH_STORAGE_RW_PARAM_OTHER_WORD_COUNT   STORAGE_RW_PARAM_OTHER_WORD_COUNT
#define FLASH_STORAGE_RW_PARAM_RESERVED_WORD_COUNT STORAGE_RW_PARAM_RESERVED_WORD_COUNT
#define FLASH_STORAGE_LOG_RECORD_COUNT       STORAGE_LOG_RECORD_COUNT
#define FLASH_STORAGE_SOC_DATA_VERSION_V2    STORAGE_SOC_DATA_VERSION_V2
#define STORAGE_FLASH_SOC_API_DECLARED       1

typedef STORAGE_SOC_DATA STORAGE_FLASH_SOC_DATA;
typedef STORAGE_RW_PARAM_DATA STORAGE_FLASH_RW_PARAM_DATA;
typedef STORAGE_FACTORY_AGING_DATA STORAGE_FLASH_FACTORY_AGING_DATA;

#define FLASH_309_RTC_RTC_VALUE              ((uint16_t)0x1222U)
#define FLASH_309_RTC_NORMAL_VALUE           ((uint16_t)0x2333U)
#define FLASH_309_NORMAL_NORMAL_VALUE        ((uint16_t)0xFFFFU)

/* App->IAP request uses the SRAM mailbox; these values are legacy compatibility constants. */
#define FLASH_TO_IAP_VALUE                   ((uint16_t)0x00ABU)
#define FLASH_TO_APP_VALUE                   ((uint16_t)0xFFFFU)
#define FLASH_UPGRADE_PARAM_FLAG_RESET       ((uint16_t)0xFFFFU)
#define FLASH_FACTORY_AGING_RESET_VALUE      ((uint16_t)0xFFFFU)
#define FLASH_FACTORY_AGING_DONE_VALUE       STORAGE_FACTORY_AGING_STATE_DONE
#define FLASH_FACTORY_AGING_STATE_RUNNING    STORAGE_FACTORY_AGING_STATE_RUNNING
#define FLASH_FACTORY_AGING_STATE_STOPPED    STORAGE_FACTORY_AGING_STATE_STOPPED
#define FLASH_FACTORY_AGING_STATE_DONE       STORAGE_FACTORY_AGING_STATE_DONE

#define FLASH_NORMAL_SLEEP_VALUE             ((uint16_t)0x1234U)
#define FLASH_DEEP_SLEEP_VALUE               ((uint16_t)0x1235U)
#define FLASH_HICCUP_SLEEP_VALUE             ((uint16_t)0x1236U)
#define FLASH_SLEEP_CHARGER_WAKE_VALUE       ((uint16_t)0x1237U)
#define FLASH_SLEEP_RESET_VALUE              ((uint16_t)0xFFFFU)
#define BOOT_FLAG_RESET_VALUE                FLASH_SLEEP_RESET_VALUE

/* Low-level/legacy backend API. Prefer Storage.h in new code. */
FLASH_Status FlashWriteOneHalfWord(uint32_t StartAddr, uint16_t Buffer);
uint16_t FlashReadOneHalfWord(uint32_t faddr);
uint8_t AppUpgrade_RequestIap(void);

uint8_t StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data);
uint8_t StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data);
uint8_t StorageFlash_LoadAfeData(uint16_t *values, uint16_t word_count);
uint8_t StorageFlash_SaveAfeData(const uint16_t *values, uint16_t word_count);
uint8_t StorageFlash_LoadRwParamData(STORAGE_FLASH_RW_PARAM_DATA *data);
uint8_t StorageFlash_SaveRwParamData(const STORAGE_FLASH_RW_PARAM_DATA *data);
uint8_t StorageFlash_LoadLogData(uint8_t *point,
                                 uint8_t records[FLASH_STORAGE_LOG_RECORD_COUNT][2]);
uint8_t StorageFlash_SaveLogData(uint8_t point,
                                 const uint8_t records[FLASH_STORAGE_LOG_RECORD_COUNT][2]);
uint8_t StorageFlash_LoadFactoryAgingData(STORAGE_FLASH_FACTORY_AGING_DATA *data);
uint8_t StorageFlash_SaveFactoryAgingData(const STORAGE_FLASH_FACTORY_AGING_DATA *data);
uint8_t StorageFlash_IsBusy(void);

void StorageFlash_PrintBootCheck(void);
void App_FlashUpdate(void);
void APP_To_IAP_Jump(void);
void InitAreaSelect(void);

#endif /* PROJECT_STORAGE_FLASH_H */
