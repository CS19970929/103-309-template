#ifndef BMS_FLASH_LAYOUT_H
#define BMS_FLASH_LAYOUT_H

#include <stdint.h>

#define BMS_FLASH_IAP_START              ((uint32_t)0x08000000UL)
#define BMS_FLASH_APP_START              ((uint32_t)0x08004800UL)
#define BMS_FLASH_PARAM_START            ((uint32_t)0x0801C000UL)
#define BMS_FLASH_APP_SIZE               ((uint32_t)(BMS_FLASH_PARAM_START - BMS_FLASH_APP_START))

#define BMS_FLASH_AFE_PARAM_SLOT_A       ((uint32_t)0x0801C000UL)
#define BMS_FLASH_RW_PARAM_SLOT_A        ((uint32_t)0x0801C400UL)
#define BMS_FLASH_AFE_PARAM_SLOT_B       ((uint32_t)0x0801C800UL)
#define BMS_FLASH_RW_PARAM_SLOT_B        ((uint32_t)0x0801CC00UL)
#define BMS_FLASH_LOG_SLOT_A             ((uint32_t)0x0801D000UL)
#define BMS_FLASH_LOG_SLOT_B             ((uint32_t)0x0801D800UL)
#define BMS_FLASH_SOC_SLOT_A             ((uint32_t)0x0801E000UL)
#define BMS_FLASH_SOC_SLOT_B             ((uint32_t)0x0801E800UL)
#define BMS_FLASH_UPGRADE_PARAM          ((uint32_t)0x0801F000UL)
#define BMS_FLASH_IAP_UPDATE_FLAG        ((uint32_t)0x0801F800UL)
#define BMS_FLASH_SLEEP_FLAG             ((uint32_t)0x0801FC00UL)

#if ((BMS_FLASH_APP_START + BMS_FLASH_APP_SIZE) != BMS_FLASH_PARAM_START)
#error "Invalid App flash boundary"
#endif

#endif
