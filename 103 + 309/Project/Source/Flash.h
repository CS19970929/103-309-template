#ifndef FLASH_H
#define FLASH_H

#define FLASH_ADDR_IAP_START             0x08000000
#define FLASH_ADDR_APP_START             0x08004800

#define FLASH_ADDR_SH367309_VALUE        0x0803E000
#define FLASH_ADDR_SH367309_FLAG         0x0803E800
#define FLASH_ADDR_UPDATE_FLAG           0x0801F800
#define FLASH_ADDR_SLEEP_FLAG            0x0801FC00

#define FLASH_STORAGE_PAGE_SIZE          ((UINT32)0x00000800)
#define FLASH_STORAGE_SLOT_SIZE          FLASH_STORAGE_PAGE_SIZE

#define FLASH_ADDR_STORAGE_AFE_SLOT_A    ((UINT32)0x0803C000)
#define FLASH_ADDR_STORAGE_AFE_SLOT_B    ((UINT32)0x0803C800)
#define FLASH_ADDR_STORAGE_LOG_SLOT_A    ((UINT32)0x0803D000)
#define FLASH_ADDR_STORAGE_LOG_SLOT_B    ((UINT32)0x0803D800)
#define FLASH_ADDR_STORAGE_SOC_SLOT_A    FLASH_ADDR_SH367309_VALUE
#define FLASH_ADDR_STORAGE_SOC_SLOT_B    FLASH_ADDR_SH367309_FLAG

#define FLASH_STORAGE_AFE_WORD_COUNT     ((UINT16)24)
#define FLASH_STORAGE_LOG_RECORD_COUNT   ((UINT16)100)

#define FLASH_309_RTC_RTC_VALUE          ((UINT16)0x1222)
#define FLASH_309_RTC_NORMAL_VALUE       ((UINT16)0x2333)
#define FLASH_309_NORMAL_NORMAL_VALUE    ((UINT16)0xFFFF)

#define FLASH_TO_IAP_VALUE               ((UINT16)0x00AB)
#define FLASH_TO_APP_VALUE               ((UINT16)0xFFFF)

#define FLASH_NORMAL_SLEEP_VALUE         ((UINT16)0x1234)
#define FLASH_DEEP_SLEEP_VALUE           ((UINT16)0x1235)
#define FLASH_HICCUP_SLEEP_VALUE         ((UINT16)0x1236)
#define FLASH_SLEEP_RESET_VALUE          ((UINT16)0xFFFF)

#define BOOT_FLAG_RESET_VALUE            FLASH_SLEEP_RESET_VALUE

typedef struct
{
	UINT16 u16SocNow;
	UINT16 u16DsgSocInt;
	UINT32 u32CycleTimes;
} STORAGE_FLASH_SOC_DATA;

FLASH_Status FlashWriteOneHalfWord(uint32_t StartAddr, uint16_t Buffer);
UINT16 FlashReadOneHalfWord(UINT32 faddr);
void FlashTest(void);

UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data);
UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data);
UINT8 StorageFlash_LoadAfeData(UINT16 *values, UINT16 word_count);
UINT8 StorageFlash_SaveAfeData(const UINT16 *values, UINT16 word_count);
UINT8 StorageFlash_LoadLogData(UINT8 *point, UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2]);
UINT8 StorageFlash_SaveLogData(UINT8 point, const UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2]);

void App_FlashUpdate(void);
void APP_To_IAP_Jump(void);
void InitAreaSelect(void);

#endif
