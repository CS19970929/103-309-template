#ifndef CT_CONFIG_H
#define CT_CONFIG_H

#include <stdint.h>

#define CT_PROTOCOL_VERSION            1u
#define CT_FW_VERSION_MAJOR            0u
#define CT_FW_VERSION_MINOR            1u
#define CT_FW_VERSION_PATCH            3u
#define CT_UART_MAX_PAYLOAD            512u
#define CT_UART_DEFAULT_BAUD           115200u
#define CT_CAN_DEFAULT_BITRATE         250000u
#define CT_NODE_ID_DEFAULT             1u

#define CT_BMS_IAP_BASE_ADDR           0x08000000u
#define CT_BMS_APP_BASE_ADDR           0x08004800u

#define CT_SELF_FLASH_BASE             0x08000000u
#define CT_SELF_FLASH_SIZE             0x00080000u
#define CT_SELF_FLASH_END              (CT_SELF_FLASH_BASE + CT_SELF_FLASH_SIZE)
#define CT_SELF_FLASH_PAGE_SIZE        0x800u

#define CT_FW_CACHE_BASE               0x08010000u
#define CT_FW_META_PAGE                0x0807F800u
#define CT_FW_CACHE_LIMIT              CT_FW_META_PAGE
#define CT_FW_CACHE_SIZE               (CT_FW_CACHE_LIMIT - CT_FW_CACHE_BASE)

#define CT_IAP_BLOCK_FRAMES            32u
#define CT_IAP_BLOCK_BYTES             (CT_IAP_BLOCK_FRAMES * 8u)

#endif
