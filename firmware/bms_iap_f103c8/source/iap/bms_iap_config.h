#ifndef BMS_IAP_CONFIG_H
#define BMS_IAP_CONFIG_H

#include <stdint.h>

#define CT_PROTOCOL_VERSION             1u

#define CT_UART_DEFAULT_BAUD            115200u
#define CT_COMM_UART_PORT_USART1        1u
#define CT_COMM_UART_PORT_USART3        3u
#define CT_COMM_UART_PORT               CT_COMM_UART_PORT_USART1

#define CT_CAN_DEFAULT_BITRATE          250000u
#define CT_NODE_ID_DEFAULT              1u

#define CT_SELF_FLASH_BASE              0x08000000u
#define CT_SELF_FLASH_SIZE              0x00020000u
#define CT_SELF_FLASH_END               (CT_SELF_FLASH_BASE + CT_SELF_FLASH_SIZE)
#define CT_SELF_FLASH_PAGE_SIZE         0x400u

#define CT_SELF_IAP_BASE                0x08000000u
#define CT_SELF_IAP_SIZE                0x00004800u
#define CT_SELF_APP_BASE                0x08004800u
#define CT_SELF_APP_LIMIT               0x0801F800u
#define CT_SELF_APP_SIZE                (CT_SELF_APP_LIMIT - CT_SELF_APP_BASE)

#define CT_BOOT_MAILBOX_ADDR            0x20004FE0u
#define CT_BOOT_MAILBOX_MAGIC           0x49415031u
#define CT_BOOT_MAILBOX_REQUEST         0x5AA55AA5u

#define CT_IAP_BLOCK_FRAMES             32u
#define CT_IAP_BLOCK_BYTES              (CT_IAP_BLOCK_FRAMES * 8u)

#endif
