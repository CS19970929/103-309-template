#ifndef CAN_APP_H
#define CAN_APP_H

#include "stm32f10x.h"

/* ── CAN App command IDs ── */
#define CAN_APP_CMD_ID                 ((UINT16)0x60U)
#define CAN_APP_ACK_ID                 ((UINT16)0x61U)
#define CAN_APP_CMD_GET_STATUS         ((UINT8)0x01U)
#define CAN_APP_CMD_ENTER_IAP          ((UINT8)0x02U)
#define CAN_APP_CMD_READ_REG           ((UINT8)0x03U)
#define CAN_APP_CMD_WRITE_PREP         ((UINT8)0x04U)
#define CAN_APP_CMD_WRITE_COMMIT       ((UINT8)0x05U)
#define CAN_APP_CMD_READ_BLOCK         ((UINT8)0x06U)
#define CAN_APP_CMD_AGING_START        ((UINT8)0x07U)
#define CAN_APP_CMD_AGING_STOP         ((UINT8)0x08U)
#define CAN_APP_CMD_AGING_RESET_TIME   ((UINT8)0x09U)
#define CAN_APP_CMD_AGING_SET_HOURS    ((UINT8)0x0AU)
#define CAN_APP_CMD_READ_BLOCK_DATA    ((UINT8)0x86U)

#define CAN_APP_READ_BLOCK_MAX_WORDS   ((UINT8)120U)
#define CAN_APP_READ_BLOCK_FRAME_INTERVAL_TICKS ((UINT32)1U)
#define CAN_APP_AGING_GUARD            ((UINT8)0xA9U)
#define CAN_APP_AGING_ACTION_START     ((UINT8)0x51U)
#define CAN_APP_AGING_ACTION_STOP      ((UINT8)0x50U)
#define CAN_APP_AGING_ACTION_RESET_TIME ((UINT8)0x5AU)
#define CAN_APP_ACK_OK                 ((UINT8)0x00U)
#define CAN_APP_ACK_BAD_CMD            ((UINT8)0x01U)
#define CAN_APP_ACK_BAD_PARAM          ((UINT8)0x02U)
#define CAN_APP_ACK_FLASH_ERR          ((UINT8)0x05U)
#define CAN_APP_ACK_NO_PERMISSION      ((UINT8)0x07U)
#define CAN_APP_ACK_BMS_ERROR          ((UINT8)0x08U)
#define CAN_APP_ENTER_IAP_DELAY_TICKS  ((UINT8)20U)

#define CAN_APP_CMD_QUEUE_SIZE         ((UINT8)4U)

/* ── App runtime state (owned by Can_HDX.c) ── */
typedef struct
{
    volatile UINT8 cmd_head;
    volatile UINT8 cmd_tail;
    volatile UINT8 cmd_count;
    UINT8 cmd_queue[CAN_APP_CMD_QUEUE_SIZE][8];
    UINT8 write_pending;
    UINT16 write_addr;
    UINT8 write_value_hi;
    UINT8 enter_iap_delay_ticks;
    UINT16 read_block_words[CAN_APP_READ_BLOCK_MAX_WORDS];
    UINT8 read_block_count;
    UINT8 read_block_index;
    UINT8 read_block_active;
    UINT32 read_block_last_tick;
} CanAppRuntime;

/* ── Public API ── */
void CanApp_HandleCmd(const UINT8 data[8], CanAppRuntime *app);
void CanApp_ServiceReadBlock(UINT32 now_tick, CanAppRuntime *app, UINT32 tick, UINT8 tx_queue_count);
void CanApp_ServiceEnterIapDelay(CanAppRuntime *app);
UINT8 CanApp_IsReadBlockActive(const CanAppRuntime *app);

#endif /* CAN_APP_H */