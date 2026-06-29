#include "main.h"
#include "DebugWatch.h"
#include "CanFeidaoFrames.h"
// #include "FactoryAging.h"
#include "IrqDebug.h"
#include "debug_hub.h"
#include <string.h>


#define FEIDAO_CAN_POWER_ON_LEVEL Bit_RESET
#define FEIDAO_CAN_POWER_OFF_LEVEL Bit_SET

enum {
	FEIDAO_CAN_PERIOD_1000MS_TICKS = 10U,
	FEIDAO_CAN_PERIOD_5000MS_TICKS = 50U,
	FEIDAO_CAN_TX_TIMEOUT_TICKS = 20U
};

enum {
	FEIDAO_CAN_TX_QUEUE_SIZE = 32U,
	FEIDAO_CAN_APP_CMD_QUEUE_SIZE = 4U,
	FEIDAO_CAN_TX_SOURCE_PERIODIC = 0U,
	FEIDAO_CAN_TX_SOURCE_REQUEST = 1U,
	FEIDAO_CAN_TX_SOURCE_NONE = 0xFFU
};

enum {
	FEIDAO_CAN_APP_CMD_ID = 0x60U,
	FEIDAO_CAN_APP_ACK_ID = 0x61U,
	FEIDAO_CAN_APP_CMD_GET_STATUS = 0x01U,
	FEIDAO_CAN_APP_CMD_ENTER_IAP = 0x02U,
	FEIDAO_CAN_APP_CMD_READ_REG = 0x03U,
	FEIDAO_CAN_APP_CMD_WRITE_PREP = 0x04U,
	FEIDAO_CAN_APP_CMD_WRITE_COMMIT = 0x05U,
	FEIDAO_CAN_APP_CMD_READ_BLOCK = 0x06U,
	FEIDAO_CAN_APP_CMD_AGING_START = 0x07U,
	FEIDAO_CAN_APP_CMD_AGING_STOP = 0x08U,
	FEIDAO_CAN_APP_CMD_AGING_RESET_TIME = 0x09U,
	FEIDAO_CAN_APP_CMD_AGING_SET_HOURS = 0x0AU,
	FEIDAO_CAN_APP_CMD_READ_BLOCK_DATA = 0x86U,
	FEIDAO_CAN_APP_READ_BLOCK_MAX_WORDS = 120U,
	FEIDAO_CAN_APP_READ_BLOCK_FRAME_INTERVAL_TICKS = 1U,
	FEIDAO_CAN_APP_AGING_GUARD = 0xA9U,
	FEIDAO_CAN_APP_AGING_ACTION_START = 0x51U,
	FEIDAO_CAN_APP_AGING_ACTION_STOP = 0x50U,
	FEIDAO_CAN_APP_AGING_ACTION_RESET_TIME = 0x5AU,
	FEIDAO_CAN_APP_ACK_OK = 0x00U,
	FEIDAO_CAN_APP_ACK_BAD_CMD = 0x01U,
	FEIDAO_CAN_APP_ACK_BAD_PARAM = 0x02U,
	FEIDAO_CAN_APP_ACK_FLASH_ERR = 0x05U,
	FEIDAO_CAN_APP_ACK_NO_PERMISSION = 0x07U,
	FEIDAO_CAN_APP_ACK_BMS_ERROR = 0x08U,
	FEIDAO_CAN_APP_ENTER_IAP_DELAY_TICKS = 20U
};

#define FEIDAO_CAN_TME_FLAG(mailbox) ((UINT32)(CAN_TSR_TME0 << (mailbox)))
#define FEIDAO_CAN_RQCP_FLAG(mailbox) ((UINT32)(0x38000000U | (1UL << ((mailbox) * 8U))))

typedef struct
{
	CanTxMsg frame;
	UINT8 source;
} FeidaoCanTxItem;

typedef struct FEIDAO_CAN_TX_RUNTIME_TAG
{
	FeidaoCanTxItem queue[FEIDAO_CAN_TX_QUEUE_SIZE];
	UINT8 head;
	UINT8 tail;
	UINT8 count;
	UINT8 mailbox;
	UINT8 mailbox_source;
	UINT32 start_tick;
} FeidaoCanTxRuntime;

typedef struct FEIDAO_CAN_RUNTIME_TAG
{
	UINT32 tick;
	UINT32 last_1000ms_tick;
	UINT32 last_5000ms_tick;
	UINT8 schedule_init;
} FeidaoCanRuntime;

typedef struct FEIDAO_CAN_APP_RUNTIME_TAG
{
	volatile UINT8 cmd_head;
	volatile UINT8 cmd_tail;
	volatile UINT8 cmd_count;
	UINT8 cmd_queue[FEIDAO_CAN_APP_CMD_QUEUE_SIZE][8];
	UINT8 write_pending;
	UINT16 write_addr;
	UINT8 write_value_hi;
	UINT8 enter_iap_delay_ticks;
	UINT16 read_block_words[FEIDAO_CAN_APP_READ_BLOCK_MAX_WORDS];
	UINT8 read_block_count;
	UINT8 read_block_index;
	UINT8 read_block_active;
	UINT32 read_block_last_tick;
} FeidaoCanAppRuntime;

static FeidaoCanTxRuntime s_tx = {
	{0},
	0U,
	0U,
	0U,
	CAN_TxStatus_NoMailBox,
	FEIDAO_CAN_TX_SOURCE_NONE,
	0U
};
static FeidaoCanRuntime s_runtime;
static FeidaoCanAppRuntime s_app;

#if DEBUG_WATCH_ENABLED
void Can_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
	watch->runtime.can_tx = &s_tx;
	watch->runtime.can_runtime = &s_runtime;
	watch->runtime.can_app = &s_app;
}
#endif

static UINT8 feidao_can_tick_elapsed(UINT32 now_tick, UINT32 start_tick, UINT32 wait_ticks);
static void feidao_can_power_on(void);
static void feidao_can_power_off(void);
static void feidao_can_clear_tx_done(UINT8 mailbox);
static void feidao_can_cancel_tx(UINT8 mailbox);
static void feidao_can_abort_tx(void);
static UINT8 feidao_can_enqueue_tx(const CanTxMsg *frame, UINT8 source);
static UINT8 feidao_can_dequeue_tx(FeidaoCanTxItem *item);
static void feidao_can_clear_tx_queue(void);
static UINT8 feidao_can_queue_has_request(void);
static void feidao_can_service_tx(UINT32 now_tick);
static void feidao_can_queue_periodic_mask(UINT16 mask);
static void feidao_can_schedule_periodic(UINT32 now_tick);
static UINT8 feidao_can_app_crc_ok(const UINT8 data[8]);
static void feidao_can_app_fill_crc(UINT8 data[8]);
static UINT8 feidao_can_aging_guard_ok(const UINT8 data[8], UINT8 action);
static void feidao_can_fill_aging_ack(UINT8 *value0, UINT8 *value1);
static void feidao_can_app_send_frame(UINT8 cmd, UINT8 status_or_seq, UINT8 value0, UINT8 value1);
static UINT8 feidao_can_app_status_from_host_error(UINT8 error);
static void feidao_can_clear_app_cmd_queue(void);
static UINT8 feidao_can_take_app_cmd(UINT8 data[8]);
static void feidao_can_queue_app_cmd(const UINT8 data[8]);
static void feidao_can_handle_app_cmd_data(const UINT8 data[8]);
static void feidao_can_start_read_block_stream(UINT8 count);
static void feidao_can_stop_read_block_stream(void);
static void feidao_can_service_read_block_stream(UINT32 now_tick);
static void feidao_can_service_enter_iap_delay(void);
static void feidao_can_handle_rx_msg(const CanRxMsg *rx_msg);
static void InitCan_GPIO(void);
static void InitCan_NVIC(void);
static void InitCan_Filter(void);
static void InitCan_CAN1(void);

static UINT8 feidao_can_tick_elapsed(UINT32 now_tick, UINT32 start_tick, UINT32 wait_ticks)
{
	return (((UINT32)(now_tick - start_tick)) >= wait_ticks) ? 1U : 0U;
}


static void feidao_can_power_on(void)
{
	GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_ON_LEVEL);
}

static void feidao_can_power_off(void)
{
	GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_OFF_LEVEL);
}

static void feidao_can_clear_tx_done(UINT8 mailbox)
{
	if (mailbox < 3U)
	{
		CAN_ClearFlag(CAN1, FEIDAO_CAN_RQCP_FLAG(mailbox));
	}
}

static void feidao_can_abort_tx(void)
{
	if (s_tx.mailbox != CAN_TxStatus_NoMailBox)
	{
		feidao_can_cancel_tx(s_tx.mailbox);
		s_tx.mailbox = CAN_TxStatus_NoMailBox;
		s_tx.mailbox_source = FEIDAO_CAN_TX_SOURCE_NONE;
	}
	feidao_can_clear_tx_queue();
}

static void feidao_can_cancel_tx(UINT8 mailbox)
{
	UINT16 wait_cnt = 0U;

	if (mailbox >= 3U)
	{
		return;
	}

	CAN_CancelTransmit(CAN1, mailbox);
	while (((CAN1->TSR & FEIDAO_CAN_TME_FLAG(mailbox)) == 0U) && (wait_cnt < 1000U))
	{
		wait_cnt++;
	}
	feidao_can_clear_tx_done(mailbox);
}

static UINT8 feidao_can_enqueue_tx(const CanTxMsg *frame, UINT8 source)
{
	if ((frame == 0) || (frame->DLC > 8U) || (s_tx.count >= FEIDAO_CAN_TX_QUEUE_SIZE))
	{
		return 0U;
	}

	s_tx.queue[s_tx.tail].frame = *frame;
	s_tx.queue[s_tx.tail].source = source;
	s_tx.tail++;
	if (s_tx.tail >= FEIDAO_CAN_TX_QUEUE_SIZE)
	{
		s_tx.tail = 0U;
	}
	s_tx.count++;
	return 1U;
}

static UINT8 feidao_can_dequeue_tx(FeidaoCanTxItem *item)
{
	if ((item == 0) || (s_tx.count == 0U))
	{
		return 0U;
	}

	*item = s_tx.queue[s_tx.head];
	s_tx.head++;
	if (s_tx.head >= FEIDAO_CAN_TX_QUEUE_SIZE)
	{
		s_tx.head = 0U;
	}
	s_tx.count--;
	return 1U;
}

static void feidao_can_clear_tx_queue(void)
{
	s_tx.head = 0U;
	s_tx.tail = 0U;
	s_tx.count = 0U;
}

static UINT8 feidao_can_queue_has_request(void)
{
	UINT8 index = s_tx.head;
	UINT8 remaining = s_tx.count;

	while (remaining != 0U)
	{
		if (s_tx.queue[index].source != FEIDAO_CAN_TX_SOURCE_PERIODIC)
		{
			return 1U;
		}
		index++;
		if (index >= FEIDAO_CAN_TX_QUEUE_SIZE)
		{
			index = 0U;
		}
		remaining--;
	}

	return 0U;
}

static void feidao_can_service_tx(UINT32 now_tick)
{
	FeidaoCanTxItem item;
	UINT8 status;

	if (s_tx.mailbox != CAN_TxStatus_NoMailBox)
	{
		status = CAN_TransmitStatus(CAN1, s_tx.mailbox);
		if (status == CAN_TxStatus_Ok)
		{
			feidao_can_clear_tx_done(s_tx.mailbox);
			s_tx.mailbox = CAN_TxStatus_NoMailBox;
			s_tx.mailbox_source = FEIDAO_CAN_TX_SOURCE_NONE;
		}
		else if (status == CAN_TxStatus_Failed)
		{
			feidao_can_clear_tx_done(s_tx.mailbox);
			s_tx.mailbox = CAN_TxStatus_NoMailBox;
			s_tx.mailbox_source = FEIDAO_CAN_TX_SOURCE_NONE;
		}
		else if (feidao_can_tick_elapsed(now_tick, s_tx.start_tick, FEIDAO_CAN_TX_TIMEOUT_TICKS))
		{
			feidao_can_cancel_tx(s_tx.mailbox);
			s_tx.mailbox = CAN_TxStatus_NoMailBox;
			s_tx.mailbox_source = FEIDAO_CAN_TX_SOURCE_NONE;
		}
	}

	if ((s_tx.mailbox == CAN_TxStatus_NoMailBox) &&
		(s_tx.count != 0U))
	{
		(void)feidao_can_dequeue_tx(&item);
		s_tx.mailbox = CAN_Transmit(CAN1, &item.frame);
		if (s_tx.mailbox != CAN_TxStatus_NoMailBox)
		{
			s_tx.mailbox_source = item.source;
			s_tx.start_tick = now_tick;
			DBG_RecordCanTxFrame((item.frame.IDE == CAN_ID_STD) ? item.frame.StdId : item.frame.ExtId,
								 item.frame.IDE,
								 item.frame.RTR,
								 item.frame.DLC,
								 item.frame.Data);
		}
		else
		{
			s_tx.mailbox_source = FEIDAO_CAN_TX_SOURCE_NONE;
		}
	}
}

static UINT8 feidao_can_transmit(CanTxMsg *Msg, UINT8 source)
{
	CanTxMsg frame;

	if (Msg == 0)
	{
		return CAN_TxStatus_NoMailBox;
	}

	frame = *Msg;
	if (frame.IDE == CAN_ID_STD)
	{
		frame.StdId += ((UINT32)CAN_ADRESS_STD_ID << 7);
	}

	/* Return 0 when the frame is queued; hardware ACK is checked later. */
	return feidao_can_enqueue_tx(&frame, source) ? 0U : CAN_TxStatus_NoMailBox;
}

UINT8 Can_HDX_Transmit(CanTxMsg *Msg)
{
	return feidao_can_transmit(Msg, FEIDAO_CAN_TX_SOURCE_REQUEST);
}

UINT8 Can_HDX_TransmitPeriodic(CanTxMsg *Msg)
{
	return feidao_can_transmit(Msg, FEIDAO_CAN_TX_SOURCE_PERIODIC);
}

static void feidao_can_queue_periodic_mask(UINT16 mask)
{
	UINT16 pending = mask;
	UINT16 before;

	while (pending != 0U)
	{
		before = pending;
		if (CanFeidao_SendNextPending(&pending) == CAN_TxStatus_NoMailBox)
		{
			break;
		}
		if (pending == before)
		{
			break;
		}
	}
}

static void feidao_can_schedule_periodic(UINT32 now_tick)
{
	if (s_runtime.schedule_init == 0U)
	{
		s_runtime.schedule_init = 1U;
		s_runtime.last_1000ms_tick = now_tick;
		s_runtime.last_5000ms_tick = now_tick;
	}

	if (feidao_can_tick_elapsed(now_tick, s_runtime.last_1000ms_tick, FEIDAO_CAN_PERIOD_1000MS_TICKS))
	{
		s_runtime.last_1000ms_tick = now_tick;
		feidao_can_queue_periodic_mask(CAN_FEIDAO_1000MS_MSG_MASK);
	}
	if (feidao_can_tick_elapsed(now_tick, s_runtime.last_5000ms_tick, FEIDAO_CAN_PERIOD_5000MS_TICKS))
	{
		s_runtime.last_5000ms_tick = now_tick;
		feidao_can_queue_periodic_mask(CAN_FEIDAO_5000MS_MSG_MASK);
	}
}

static UINT8 feidao_can_app_crc_ok(const UINT8 data[8])
{
	UINT16 expect_crc = (UINT16)(((UINT16)data[6] << 8) | data[7]);
	UINT16 actual_crc = Sci_CRC16RTU((UINT8 *)data, 6U);

	return (expect_crc == actual_crc) ? 1U : 0U;
}

static void feidao_can_app_fill_crc(UINT8 data[8])
{
	UINT16 crc = Sci_CRC16RTU(data, 6U);
	data[6] = (UINT8)(crc >> 8);
	data[7] = (UINT8)crc;
}

static UINT8 feidao_can_aging_guard_ok(const UINT8 data[8], UINT8 action)
{
	return ((data[3] == FEIDAO_CAN_APP_AGING_GUARD) &&
			(data[4] == action) &&
			(data[5] == (UINT8)CAN_ADRESS_STD_ID)) ? 1U : 0U;
}

static void feidao_can_fill_aging_ack(UINT8 *value0, UINT8 *value1)
{
	// UINT32 hours;

	// if (value0 != 0)
	// {
	// 	*value0 = FactoryAging_GetState();
	// }
	// if (value1 != 0)
	// {
	// 	hours = (FactoryAging_GetRemainingSeconds() + 3599U) / 3600U;
	// 	*value1 = (hours > 0xFFU) ? 0xFFU : (UINT8)hours;
	// }
}

static void feidao_can_app_send_frame(UINT8 cmd, UINT8 status_or_seq, UINT8 value0, UINT8 value1)
{
	CanTxMsg tx_msg;

	memset(&tx_msg, 0, sizeof(tx_msg));
	tx_msg.StdId = FEIDAO_CAN_APP_ACK_ID;
	tx_msg.IDE = CAN_ID_STD;
	tx_msg.RTR = CAN_RTR_DATA;
	tx_msg.DLC = 8U;
	tx_msg.Data[0] = 0x5AU;
	tx_msg.Data[1] = 0xA5U;
	tx_msg.Data[2] = cmd;
	tx_msg.Data[3] = status_or_seq;
	tx_msg.Data[4] = value0;
	tx_msg.Data[5] = value1;
	feidao_can_app_fill_crc(tx_msg.Data);
	(void)Can_HDX_Transmit(&tx_msg);
}

static UINT8 feidao_can_app_status_from_host_error(UINT8 error)
{
	switch (error)
	{
	case 0U:
		return FEIDAO_CAN_APP_ACK_OK;
	case RS485_ERROR_NO_PERMISSION:
		return FEIDAO_CAN_APP_ACK_NO_PERMISSION;
	case RS485_ERROR_ADDR_INVALID:
	case RS485_ERROR_DATA_INVALID:
	case RS485_ERROR_RONLY_NO_W:
	case RS485_ERROR_WONLY_NO_R:
		return FEIDAO_CAN_APP_ACK_BAD_PARAM;
	default:
		return FEIDAO_CAN_APP_ACK_BMS_ERROR;
	}
}

static void feidao_can_clear_app_cmd_queue(void)
{
	__disable_irq();
	s_app.cmd_head = 0U;
	s_app.cmd_tail = 0U;
	s_app.cmd_count = 0U;
	__enable_irq();
}

static UINT8 feidao_can_take_app_cmd(UINT8 data[8])
{
	UINT8 has_cmd = 0U;

	__disable_irq();
	if (s_app.cmd_count != 0U)
	{
		memcpy(data, s_app.cmd_queue[s_app.cmd_head], 8U);
		s_app.cmd_head++;
		if (s_app.cmd_head >= FEIDAO_CAN_APP_CMD_QUEUE_SIZE)
		{
			s_app.cmd_head = 0U;
		}
		s_app.cmd_count--;
		has_cmd = 1U;
	}
	__enable_irq();

	return has_cmd;
}

static void feidao_can_queue_app_cmd(const UINT8 data[8])
{
	if (s_app.cmd_count >= FEIDAO_CAN_APP_CMD_QUEUE_SIZE)
	{
		return;
	}

	memcpy(s_app.cmd_queue[s_app.cmd_tail], data, 8U);
	s_app.cmd_tail++;
	if (s_app.cmd_tail >= FEIDAO_CAN_APP_CMD_QUEUE_SIZE)
	{
		s_app.cmd_tail = 0U;
	}
	s_app.cmd_count++;
}

static void feidao_can_start_read_block_stream(UINT8 count)
{
	s_app.read_block_count = count;
	s_app.read_block_index = 0U;
	s_app.read_block_active = 1U;
	s_app.read_block_last_tick = s_runtime.tick - FEIDAO_CAN_APP_READ_BLOCK_FRAME_INTERVAL_TICKS;
}

static void feidao_can_stop_read_block_stream(void)
{
	s_app.read_block_active = 0U;
	s_app.read_block_count = 0U;
	s_app.read_block_index = 0U;
}

static void feidao_can_service_read_block_stream(UINT32 now_tick)
{
	if (s_app.read_block_active == 0U)
	{
		return;
	}
	if (s_app.read_block_index >= s_app.read_block_count)
	{
		feidao_can_stop_read_block_stream();
		return;
	}
	if (s_tx.count > (FEIDAO_CAN_TX_QUEUE_SIZE - 4U))
	{
		return;
	}
	if (feidao_can_tick_elapsed(now_tick,
								s_app.read_block_last_tick,
								FEIDAO_CAN_APP_READ_BLOCK_FRAME_INTERVAL_TICKS) == 0U)
	{
		return;
	}

	feidao_can_app_send_frame(FEIDAO_CAN_APP_CMD_READ_BLOCK_DATA,
							  s_app.read_block_index,
							  (UINT8)(s_app.read_block_words[s_app.read_block_index] >> 8),
							  (UINT8)s_app.read_block_words[s_app.read_block_index]);
	s_app.read_block_index++;
	s_app.read_block_last_tick = now_tick;
	if (s_app.read_block_index >= s_app.read_block_count)
	{
		feidao_can_stop_read_block_stream();
	}
}

static void feidao_can_handle_app_cmd_data(const UINT8 data[8])
{
	UINT8 status = FEIDAO_CAN_APP_ACK_OK;
	UINT8 value0 = 0U;
	UINT8 value1 = 0U;
	UINT8 cmd;
	UINT16 reg_addr;
	UINT16 reg_value;
	UINT8 reg_count;
	UINT8 host_error;

	if ((data[0] != 0xA5U) ||
		(data[1] != 0x5AU) ||
		(feidao_can_app_crc_ok(data) == 0U))
	{
		return;
	}

	cmd = data[2];
	feidao_can_stop_read_block_stream();

	switch (cmd)
	{
	case FEIDAO_CAN_APP_CMD_GET_STATUS:
		reg_value = g_stCellInfoReport.SocElement.u16Soc;
		value0 = (UINT8)((reg_value > 100U) ? 100U : reg_value);
		reg_value = g_stCellInfoReport.SocElement.u16Soh;
		value1 = (UINT8)((reg_value > 100U) ? 100U : reg_value);
		break;

	case FEIDAO_CAN_APP_CMD_ENTER_IAP:
		if ((data[3] != 0xC3U) ||
			(data[4] != 0x3CU) ||
			(data[5] != (UINT8)CAN_ADRESS_STD_ID))
		{
			status = FEIDAO_CAN_APP_ACK_BAD_PARAM;
			break;
		}
		if (AppUpgrade_RequestIap() == 0U)
		{
			status = FEIDAO_CAN_APP_ACK_FLASH_ERR;
			break;
		}
		value0 = 0x08U;
		value1 = 0x48U;
		s_app.enter_iap_delay_ticks = FEIDAO_CAN_APP_ENTER_IAP_DELAY_TICKS;
		break;

	case FEIDAO_CAN_APP_CMD_READ_REG:
		reg_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		host_error = Sci_HostReadWords(reg_addr, 1U, &reg_value);
		status = feidao_can_app_status_from_host_error(host_error);
		if (status == FEIDAO_CAN_APP_ACK_OK)
		{
			value0 = (UINT8)(reg_value >> 8);
			value1 = (UINT8)reg_value;
		}
		break;

	case FEIDAO_CAN_APP_CMD_READ_BLOCK:
		reg_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		reg_count = data[5];
		if ((reg_count == 0U) ||
			(reg_count > FEIDAO_CAN_APP_READ_BLOCK_MAX_WORDS) ||
			(((UINT32)reg_addr + (UINT32)reg_count - 1U) > (UINT32)0xFFFFU))
		{
			status = FEIDAO_CAN_APP_ACK_BAD_PARAM;
			break;
		}
		host_error = Sci_HostReadWords(reg_addr, reg_count, s_app.read_block_words);
		status = feidao_can_app_status_from_host_error(host_error);
		if (status == FEIDAO_CAN_APP_ACK_OK)
		{
			value0 = reg_count;
			value1 = 0U;
			feidao_can_start_read_block_stream(reg_count);
		}
		break;

	case FEIDAO_CAN_APP_CMD_WRITE_PREP:
		s_app.write_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		s_app.write_value_hi = data[5];
		s_app.write_pending = 1U;
		value0 = data[3];
		value1 = data[4];
		break;

	case FEIDAO_CAN_APP_CMD_WRITE_COMMIT:
		reg_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		if ((s_app.write_pending == 0U) || (reg_addr != s_app.write_addr))
		{
			s_app.write_pending = 0U;
			status = FEIDAO_CAN_APP_ACK_BAD_PARAM;
			break;
		}
		reg_value = (UINT16)(((UINT16)s_app.write_value_hi << 8) | data[5]);
		s_app.write_pending = 0U;
		host_error = Sci_HostWriteWords(reg_addr, &reg_value, 1U);
		status = feidao_can_app_status_from_host_error(host_error);
		break;
#if 0
	case FEIDAO_CAN_APP_CMD_AGING_START:
		if (feidao_can_aging_guard_ok(data, FEIDAO_CAN_APP_AGING_ACTION_START) == 0U)
		{
			status = FEIDAO_CAN_APP_ACK_BAD_PARAM;
			break;
		}
		if (FactoryAging_StartByHost() == 0U)
		{
			status = FEIDAO_CAN_APP_ACK_BMS_ERROR;
			break;
		}
		feidao_can_fill_aging_ack(&value0, &value1);
		break;

	case FEIDAO_CAN_APP_CMD_AGING_STOP:
		if (feidao_can_aging_guard_ok(data, FEIDAO_CAN_APP_AGING_ACTION_STOP) == 0U)
		{
			status = FEIDAO_CAN_APP_ACK_BAD_PARAM;
			break;
		}
		if (FactoryAging_StopByHost() == 0U)
		{
			status = FEIDAO_CAN_APP_ACK_BMS_ERROR;
			break;
		}
		feidao_can_fill_aging_ack(&value0, &value1);
		break;

	case FEIDAO_CAN_APP_CMD_AGING_RESET_TIME:
		if (feidao_can_aging_guard_ok(data, FEIDAO_CAN_APP_AGING_ACTION_RESET_TIME) == 0U)
		{
			status = FEIDAO_CAN_APP_ACK_BAD_PARAM;
			break;
		}
		if (FactoryAging_ResetTimeByHost() == 0U)
		{
			status = FEIDAO_CAN_APP_ACK_BMS_ERROR;
			break;
		}
		feidao_can_fill_aging_ack(&value0, &value1);
		break;

	case FEIDAO_CAN_APP_CMD_AGING_SET_HOURS:
		if ((data[3] != FEIDAO_CAN_APP_AGING_GUARD) ||
			(data[5] != (UINT8)CAN_ADRESS_STD_ID) ||
			(data[4] < (UINT8)FACTORY_AGING_DURATION_HOURS_MIN) ||
			(data[4] > (UINT8)FACTORY_AGING_DURATION_HOURS_MAX))
		{
			status = FEIDAO_CAN_APP_ACK_BAD_PARAM;
			break;
		}
		if (FactoryAging_SetDurationHoursByHost((UINT16)data[4]) == 0U)
		{
			status = FEIDAO_CAN_APP_ACK_BMS_ERROR;
			break;
		}
		feidao_can_fill_aging_ack(&value0, &value1);
		break;
#endif

	default:
		status = FEIDAO_CAN_APP_ACK_BAD_CMD;
		break;
	}

	feidao_can_app_send_frame(cmd, status, value0, value1);
}

static void feidao_can_service_enter_iap_delay(void)
{
	if ((s_app.enter_iap_delay_ticks == 0U) || (0 == g_st_SysTimeFlag.bits.b1Sys10msFlag))
	{
		return;
	}

	s_app.enter_iap_delay_ticks--;
	if (s_app.enter_iap_delay_ticks == 0U)
	{
		u8FlashUpdateFlag = 1U;
	}
}

static void feidao_can_handle_rx_msg(const CanRxMsg *rx_msg)
{
	UINT16 expect_std_id = (UINT16)(((UINT16)CAN_ADRESS_STD_ID << 7) | FEIDAO_CAN_APP_CMD_ID);

	if ((rx_msg == 0) || (rx_msg->IDE != CAN_ID_STD))
	{
		return;
	}

	if (((UINT16)rx_msg->StdId == expect_std_id) && (rx_msg->DLC == 8U))
	{
		feidao_can_queue_app_cmd(rx_msg->Data);
	}
}

static void InitCan_GPIO(void)
{
	GPIO_InitTypeDef gpio;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

	GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_ON_LEVEL);
	gpio.GPIO_Pin = PIN_CMNT_EN;
	gpio.GPIO_Mode = GPIO_Mode_Out_PP;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIO_CMNT_EN, &gpio);
	feidao_can_power_on();

	gpio.GPIO_Pin = GPIO_Pin_11;
	gpio.GPIO_Mode = GPIO_Mode_IPU;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &gpio);

	gpio.GPIO_Pin = GPIO_Pin_12;
	gpio.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &gpio);
}

static void InitCan_NVIC(void)
{
	NVIC_InitTypeDef nvic;

	nvic.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 1U;
	nvic.NVIC_IRQChannelSubPriority = 1U;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);
}

static void InitCan_Filter(void)
{
	CAN_FilterInitTypeDef filter;

	filter.CAN_FilterNumber = 0U;
	filter.CAN_FilterMode = CAN_FilterMode_IdMask;
	filter.CAN_FilterScale = CAN_FilterScale_32bit;
	filter.CAN_FilterIdHigh = 0U;
	filter.CAN_FilterIdLow = 0U;
	filter.CAN_FilterMaskIdHigh = 0U;
	filter.CAN_FilterMaskIdLow = 0U;
	filter.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
	filter.CAN_FilterActivation = ENABLE;
	CAN_FilterInit(&filter);
}

static void InitCan_CAN1(void)
{
	CAN_InitTypeDef can;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
	CAN_DeInit(CAN1);
	CAN_StructInit(&can);
	can.CAN_TTCM = DISABLE;
	can.CAN_ABOM = ENABLE;
	can.CAN_AWUM = DISABLE;
	can.CAN_NART = ENABLE;
	can.CAN_RFLM = DISABLE;
	can.CAN_TXFP = DISABLE;
	can.CAN_Mode = CAN_Mode_Normal;
	can.CAN_SJW = CAN_SJW_1tq;
	can.CAN_BS1 = CAN_BS1_5tq;
	can.CAN_BS2 = CAN_BS2_2tq;
	can.CAN_Prescaler = 4U;
	(void)CAN_Init(CAN1, &can);
	CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);
}

void InitCan(void)
{
	s_tx.mailbox = CAN_TxStatus_NoMailBox;
	s_tx.mailbox_source = FEIDAO_CAN_TX_SOURCE_NONE;
	s_runtime.schedule_init = 0U;
	s_app.enter_iap_delay_ticks = 0U;
	s_app.read_block_active = 0U;
	feidao_can_clear_tx_queue();
	feidao_can_clear_app_cmd_queue();
	InitCan_GPIO();
	InitCan_NVIC();
	InitCan_CAN1();
	InitCan_Filter();
	feidao_can_power_on();
}

static UINT8 can_has_pending_work(void)
{
	if (s_tx.count != 0U)
	{
		return 1U;
	}
	if (s_tx.mailbox != CAN_TxStatus_NoMailBox)
	{
		return 1U;
	}
	if (s_app.read_block_active != 0U)
	{
		return 1U;
	}
	if (s_app.cmd_count != 0U)
	{
		return 1U;
	}
	return ((CAN1->TSR & CAN_TSR_TME) != CAN_TSR_TME) ? 1U : 0U;
}

static UINT8 can_has_sleep_blocking_work(void)
{
	if (feidao_can_queue_has_request() != 0U)
	{
		return 1U;
	}
	if ((s_tx.mailbox != CAN_TxStatus_NoMailBox) &&
		(s_tx.mailbox_source != FEIDAO_CAN_TX_SOURCE_PERIODIC))
	{
		return 1U;
	}
	if (s_app.read_block_active != 0U)
	{
		return 1U;
	}
	if (s_app.cmd_count != 0U)
	{
		return 1U;
	}
	if ((s_tx.mailbox == CAN_TxStatus_NoMailBox) &&
		((CAN1->TSR & CAN_TSR_TME) != CAN_TSR_TME))
	{
		return 1U;
	}
	return 0U;
}

UINT8 Can_PeekBusy(void)
{
	if (can_has_pending_work() != 0U)
	{
		return 1U;
	}
	return (sys_time.last_ext_comm_cnt_can != sys_time.can_rcv_cnt) ? 1U : 0U;
}

UINT8 Can_IsBusy(void)
{
	if (can_has_sleep_blocking_work() != 0U)
	{
		return 1U;
	}
	if(sys_time.last_ext_comm_cnt_can != sys_time.can_rcv_cnt)
	{
		sys_time.last_ext_comm_cnt_can = sys_time.can_rcv_cnt;
		return 1U;	
	}
	return 0U;
}

void Can_PrepareSleep(void)
{
	feidao_can_abort_tx();
	feidao_can_clear_app_cmd_queue();
	feidao_can_stop_read_block_stream();
	feidao_can_power_off();
}

void App_Can(void)
{
	UINT32 now_tick = SysTime_Get10msTickCount();
	UINT8 app_cmd_data[8];

	s_runtime.tick = now_tick;
	feidao_can_schedule_periodic(now_tick);
	if (feidao_can_take_app_cmd(app_cmd_data) != 0U)
	{
		feidao_can_handle_app_cmd_data(app_cmd_data);
	}
	feidao_can_service_read_block_stream(now_tick);
	feidao_can_service_tx(now_tick);
	feidao_can_service_enter_iap_delay();
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
	CanRxMsg rx_msg;

	IrqDebug_CountFast((uint8_t)IRQDBG_CAN1_RX0);
	while (CAN_MessagePending(CAN1, CAN_FIFO0) != 0U)
	{
		sys_time.can_rcv_cnt++;
		CAN_Receive(CAN1, CAN_FIFO0, &rx_msg);
		DBG_RecordCanRxFrame((rx_msg.IDE == CAN_ID_STD) ? rx_msg.StdId : rx_msg.ExtId,
							 rx_msg.IDE,
							 rx_msg.RTR,
							 rx_msg.DLC,
							 rx_msg.Data);
		feidao_can_handle_rx_msg(&rx_msg);
	}
}

#if PROJECT_CFG_DEBUG_MONITOR_ENABLE
void Can_GetDebugSnapshot(uint8_t *power_on,
                          uint8_t *bus_off,
                          uint8_t *tx_queue,
                          uint16_t *esr)
{
	if (power_on  != 0)   *power_on    = (uint8_t)(GPIO_ReadOutputDataBit(GPIO_CMNT_EN, PIN_CMNT_EN) == FEIDAO_CAN_POWER_ON_LEVEL);
	if (bus_off   != 0)   *bus_off     = (uint8_t)((CAN1->ESR & CAN_ESR_BOFF) != 0U);
	if (tx_queue  != 0)   *tx_queue    = s_tx.count;
	if (esr       != 0)   *esr         = (uint16_t)(CAN1->ESR & 0xFFFFU);
}
#endif
