#ifndef IAP_CONFIG_H
#define IAP_CONFIG_H

#include <stdint.h>

#define IAP_FLASH_BASE                 ((uint32_t)0x08000000U)
#define IAP_APP_BASE                   ((uint32_t)0x08004800U)
#define IAP_APP_MAX_END_EXCLUSIVE      ((uint32_t)0x0801C000U)
#define IAP_BOOT_MAX_END_EXCLUSIVE     IAP_APP_BASE

#define IAP_UPDATE_FLAG_ADDR           ((uint32_t)0x0801F800U)
#define IAP_FLASH_TO_IAP_VALUE         ((uint16_t)0x00ABU)
#define IAP_FLASH_TO_APP_VALUE         ((uint16_t)0xFFFFU)

/* This project is built as STM32F10X_MD, so Flash pages are 1 KB. */
#define IAP_FLASH_PAGE_SIZE            ((uint32_t)0x00000400U)

#define IAP_BOOT_WAIT_MS               ((uint32_t)1500U)
#define IAP_FINAL_RESET_DELAY_MS       ((uint32_t)200U)

#define FEIDAO_NODE_IOT                ((uint8_t)0x10U)
#define FEIDAO_NODE_BATTERY            ((uint8_t)0x14U)
#define FEIDAO_NODE_BROADCAST          ((uint8_t)0x1FU)

#define IAP_CAN_HOST_NODE_DEFAULT      FEIDAO_NODE_IOT
#define IAP_CAN_DEVICE_NODE            FEIDAO_NODE_BATTERY

#define FEIDAO_CTRL_WRITE              ((uint8_t)0x00U)
#define FEIDAO_CTRL_ACK                ((uint8_t)0x02U)
#define FEIDAO_CTRL_LONG_START         ((uint8_t)0x04U)
#define FEIDAO_CTRL_LONG_DATA          ((uint8_t)0x05U)
#define FEIDAO_CTRL_LONG_END           ((uint8_t)0x06U)

#define FEIDAO_UPGRADE_START_INDEX     ((uint8_t)0x04U)
#define FEIDAO_UPGRADE_DATA_INDEX      ((uint8_t)0x05U)
#define FEIDAO_UPGRADE_START_ACK_CHD   ((uint8_t)0x01U)
#define FEIDAO_UPGRADE_CHUNK_CHD       ((uint8_t)0x00U)

#define IAP_LONG_PACKET_MAX_FRAMES     ((uint16_t)256U)
#define IAP_LONG_PACKET_BYTES          ((uint32_t)2048U)

#define IAP_STATUS_CHUNK_OK            ((uint8_t)0x00U)
#define IAP_STATUS_CHUNK_CRC_ERROR     ((uint8_t)0x01U)
#define IAP_STATUS_FILE_CRC_ERROR      ((uint8_t)0x02U)
#define IAP_STATUS_DONE                ((uint8_t)0x03U)
#define IAP_STATUS_OTHER_ERROR         ((uint8_t)0xFFU)

#endif
