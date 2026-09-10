#include "main.h"
#include <string.h>

#define CAN_POWER_ON_LEVEL Bit_RESET
#define CAN_POWER_OFF_LEVEL Bit_SET

#define HDX_CAN_FRAME_ID_MIN ((UINT8)0x00U)
#define HDX_CAN_FRAME_ID_MAX ((UINT8)0x11U)
#define HDX_CAN_FRAME_DATA_LEN ((UINT8)8U)

#define COMM_TOOL_CAN_CMD_ID ((UINT8)0x60U)
#define COMM_TOOL_CAN_ACK_ID ((UINT8)0x61U)

#define CAN_TME_FLAG(mailbox) ((UINT32)(CAN_TSR_TME0 << (mailbox)))
#define CAN_RQCP_FLAG(mailbox) ((UINT32)(0x38000000U | (1UL << ((mailbox) * 8U))))

enum
{
	CAN_TX_TIMEOUT_TICKS = 20U,
	CAN_TX_QUEUE_SIZE = 32U,
	COMM_TOOL_CAN_CMD_QUEUE_SIZE = 4U,
	CAN_TX_SOURCE_PERIODIC = 0U,
	CAN_TX_SOURCE_REQUEST = 1U,
	CAN_TX_SOURCE_NONE = 0xFFU
};

enum
{
	COMM_TOOL_CAN_CMD_GET_STATUS = 0x01U,
	COMM_TOOL_CAN_CMD_ENTER_IAP = 0x02U,
	COMM_TOOL_CAN_CMD_READ_REG = 0x03U,
	COMM_TOOL_CAN_CMD_WRITE_PREP = 0x04U,
	COMM_TOOL_CAN_CMD_WRITE_COMMIT = 0x05U,
	COMM_TOOL_CAN_CMD_READ_BLOCK = 0x06U,
	COMM_TOOL_CAN_CMD_READ_BLOCK_DATA = 0x86U,
	COMM_TOOL_CAN_READ_BLOCK_MAX_WORDS = 120U,
	COMM_TOOL_CAN_READ_BLOCK_FRAME_INTERVAL_TICKS = 1U,
	COMM_TOOL_CAN_ACK_OK = 0x00U,
	COMM_TOOL_CAN_ACK_BAD_CMD = 0x01U,
	COMM_TOOL_CAN_ACK_BAD_PARAM = 0x02U,
	COMM_TOOL_CAN_ACK_FLASH_ERR = 0x05U,
	COMM_TOOL_CAN_ACK_NO_PERMISSION = 0x07U,
	COMM_TOOL_CAN_ACK_BMS_ERROR = 0x08U,
	COMM_TOOL_CAN_ENTER_IAP_DELAY_TICKS = 20U
};

typedef struct
{
	CanTxMsg frame;
	UINT8 source;
} CanTxItem;

typedef struct
{
	CanTxItem queue[CAN_TX_QUEUE_SIZE];
	UINT8 head;
	UINT8 tail;
	UINT8 count;
	UINT8 mailbox;
	UINT8 mailbox_source;
	UINT32 start_tick;
} CanTxRuntime;

typedef struct
{
	UINT32 tick;
} CanRuntime;

typedef struct
{
	volatile UINT8 cmd_head;
	volatile UINT8 cmd_tail;
	volatile UINT8 cmd_count;
	UINT8 cmd_queue[COMM_TOOL_CAN_CMD_QUEUE_SIZE][8];
	UINT8 write_pending;
	UINT16 write_addr;
	UINT8 write_value_hi;
	UINT8 enter_iap_delay_ticks;
	UINT16 read_block_words[COMM_TOOL_CAN_READ_BLOCK_MAX_WORDS];
	UINT8 read_block_count;
	UINT8 read_block_index;
	UINT8 read_block_active;
	UINT32 read_block_last_tick;
} CommToolCanRuntime;

static CanTxRuntime s_tx;
static CanRuntime s_runtime;
static CommToolCanRuntime s_comm_tool;
static volatile UINT32 s_hdx_pending_mask;

static UINT8 can_tick_elapsed(UINT32 now_tick, UINT32 start_tick, UINT32 wait_ticks);
static void can_power_on(void);
static void can_power_off(void);
static void can_clear_tx_done(UINT8 mailbox);
static void can_cancel_tx(UINT8 mailbox);
static void can_mark_tx_idle(void);
static void can_abort_tx(void);
static UINT8 can_enqueue_tx(const CanTxMsg *frame, UINT8 source);
static UINT8 can_dequeue_tx(CanTxItem *item);
static void can_clear_tx_queue(void);
static UINT8 can_queue_has_request(void);
static void can_service_tx(UINT32 now_tick);
static UINT8 can_transmit(CanTxMsg *msg, UINT8 source);

static void hdx_put_u16_be(UINT8 *data, UINT8 offset, UINT16 value);
static void hdx_fill_crc(CanTxMsg *msg);
static void hdx_init_frame(CanTxMsg *msg, UINT8 frame_id);
static UINT8 hdx_send_frame_00(void);
static UINT8 hdx_send_frame_01(void);
static UINT8 hdx_send_frame_02(void);
static UINT8 hdx_send_frame_03(void);
static UINT8 hdx_send_frame_04(void);
static UINT8 hdx_send_frame_05(void);
static UINT8 hdx_send_frame_06(void);
static UINT8 hdx_send_cell_frame(UINT8 frame_id, UINT8 first_cell);
static UINT8 hdx_send_frame(UINT8 frame_id);
static UINT8 hdx_queue_request(UINT16 std_id);
static UINT8 hdx_has_pending_request(void);
static void hdx_clear_pending_requests(void);
static void hdx_process_next_request(void);

static UINT8 comm_tool_crc_ok(const UINT8 data[8]);
static void comm_tool_fill_crc(UINT8 data[8]);
static void comm_tool_send_frame(UINT8 cmd, UINT8 status_or_seq, UINT8 value0, UINT8 value1);
static UINT8 comm_tool_status_from_host_error(UINT8 error);
static void comm_tool_clear_cmd_queue(void);
static UINT8 comm_tool_take_cmd(UINT8 data[8]);
static void comm_tool_queue_cmd(const UINT8 data[8]);
static void comm_tool_handle_cmd_data(const UINT8 data[8]);
static void comm_tool_start_read_block_stream(UINT8 count);
static void comm_tool_stop_read_block_stream(void);
static void comm_tool_service_read_block_stream(UINT32 now_tick);
static void comm_tool_service_enter_iap_delay(void);
static void can_handle_rx_msg(const CanRxMsg *rx_msg);

static void InitCan_GPIO(void);
static void InitCan_NVIC(void);
static void InitCan_Filter(void);
static void InitCan_CAN1(void);

static UINT8 can_tick_elapsed(UINT32 now_tick, UINT32 start_tick, UINT32 wait_ticks)
{
	return (((UINT32)(now_tick - start_tick)) >= wait_ticks) ? 1U : 0U;
}

static void can_power_on(void)
{
	GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, CAN_POWER_ON_LEVEL);
}

static void can_power_off(void)
{
	GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, CAN_POWER_OFF_LEVEL);
}

static void can_clear_tx_done(UINT8 mailbox)
{
	if (mailbox < 3U)
	{
		CAN_ClearFlag(CAN1, CAN_RQCP_FLAG(mailbox));
	}
}

static void can_cancel_tx(UINT8 mailbox)
{
	UINT16 wait_cnt = 0U;

	if (mailbox >= 3U)
	{
		return;
	}

	CAN_CancelTransmit(CAN1, mailbox);
	while (((CAN1->TSR & CAN_TME_FLAG(mailbox)) == 0U) && (wait_cnt < 1000U))
	{
		wait_cnt++;
	}
	can_clear_tx_done(mailbox);
}

static void can_mark_tx_idle(void)
{
	s_tx.mailbox = CAN_TxStatus_NoMailBox;
	s_tx.mailbox_source = CAN_TX_SOURCE_NONE;
}

static void can_abort_tx(void)
{
	if (s_tx.mailbox != CAN_TxStatus_NoMailBox)
	{
		can_cancel_tx(s_tx.mailbox);
		can_mark_tx_idle();
	}
	can_clear_tx_queue();
}

static UINT8 can_enqueue_tx(const CanTxMsg *frame, UINT8 source)
{
	if ((frame == 0) || (frame->DLC > 8U) || (s_tx.count >= CAN_TX_QUEUE_SIZE))
	{
		return 0U;
	}

	s_tx.queue[s_tx.tail].frame = *frame;
	s_tx.queue[s_tx.tail].source = source;
	s_tx.tail++;
	if (s_tx.tail >= CAN_TX_QUEUE_SIZE)
	{
		s_tx.tail = 0U;
	}
	s_tx.count++;
	return 1U;
}

static UINT8 can_dequeue_tx(CanTxItem *item)
{
	if ((item == 0) || (s_tx.count == 0U))
	{
		return 0U;
	}

	*item = s_tx.queue[s_tx.head];
	s_tx.head++;
	if (s_tx.head >= CAN_TX_QUEUE_SIZE)
	{
		s_tx.head = 0U;
	}
	s_tx.count--;
	return 1U;
}

static void can_clear_tx_queue(void)
{
	s_tx.head = 0U;
	s_tx.tail = 0U;
	s_tx.count = 0U;
}

static UINT8 can_queue_has_request(void)
{
	UINT8 index = s_tx.head;
	UINT8 remaining = s_tx.count;

	while (remaining != 0U)
	{
		if (s_tx.queue[index].source != CAN_TX_SOURCE_PERIODIC)
		{
			return 1U;
		}
		index++;
		if (index >= CAN_TX_QUEUE_SIZE)
		{
			index = 0U;
		}
		remaining--;
	}

	return 0U;
}

static void can_service_tx(UINT32 now_tick)
{
	CanTxItem item;
	UINT8 status;

	if (s_tx.mailbox != CAN_TxStatus_NoMailBox)
	{
		status = CAN_TransmitStatus(CAN1, s_tx.mailbox);
		if ((status == CAN_TxStatus_Ok) || (status == CAN_TxStatus_Failed))
		{
			can_clear_tx_done(s_tx.mailbox);
			can_mark_tx_idle();
		}
		else if (can_tick_elapsed(now_tick, s_tx.start_tick, CAN_TX_TIMEOUT_TICKS))
		{
			can_cancel_tx(s_tx.mailbox);
			can_mark_tx_idle();
		}
	}

	if ((s_tx.mailbox == CAN_TxStatus_NoMailBox) && (s_tx.count != 0U))
	{
		(void)can_dequeue_tx(&item);
		s_tx.mailbox = CAN_Transmit(CAN1, &item.frame);
		if (s_tx.mailbox != CAN_TxStatus_NoMailBox)
		{
			s_tx.mailbox_source = item.source;
			s_tx.start_tick = now_tick;
		}
		else
		{
			can_mark_tx_idle();
		}
	}
}

static UINT8 can_transmit(CanTxMsg *msg, UINT8 source)
{
	CanTxMsg frame;

	if (msg == 0)
	{
		return CAN_TxStatus_NoMailBox;
	}

	frame = *msg;
	if (frame.IDE == CAN_ID_STD)
	{
		frame.StdId += ((UINT32)CAN_ADRESS_STD_ID << 7);
	}

	/* Return 0 when the frame is queued; hardware ACK is checked later. */
	return can_enqueue_tx(&frame, source) ? 0U : CAN_TxStatus_NoMailBox;
}

UINT8 Can_HDX_Transmit(CanTxMsg *msg)
{
	return can_transmit(msg, CAN_TX_SOURCE_REQUEST);
}

UINT8 Can_HDX_TransmitPeriodic(CanTxMsg *msg)
{
	return can_transmit(msg, CAN_TX_SOURCE_PERIODIC);
}

static void hdx_put_u16_be(UINT8 *data, UINT8 offset, UINT16 value)
{
	data[offset] = (UINT8)((value >> 8) & 0xFFU);
	data[offset + 1U] = (UINT8)(value & 0xFFU);
}

static void hdx_fill_crc(CanTxMsg *msg)
{
	UINT16 crc = Sci_CRC16RTU(msg->Data, 6U);
	hdx_put_u16_be(msg->Data, 6U, crc);
}

static void hdx_init_frame(CanTxMsg *msg, UINT8 frame_id)
{
	memset(msg, 0, sizeof(*msg));
	msg->StdId = frame_id;
	msg->ExtId = 0U;
	msg->IDE = CAN_ID_STD;
	msg->RTR = CAN_RTR_DATA;
	msg->DLC = HDX_CAN_FRAME_DATA_LEN;
}

static UINT8 hdx_send_frame_00(void)
{
	CanTxMsg msg;
	UINT16 value;

	hdx_init_frame(&msg, 0x00U);
	hdx_put_u16_be(msg.Data, 0U, g_stCellInfoReport.u16VCellTotle);

	if (g_stCellInfoReport.u16IDischg > 0U)
	{
		value = (UINT16)(g_stCellInfoReport.u16IDischg * 10U);
		value = (UINT16)((0x7FFFU - value + 1U) | 0x8000U);
	}
	else
	{
		value = (UINT16)(g_stCellInfoReport.u16Ichg * 10U);
	}
	hdx_put_u16_be(msg.Data, 2U, value);
	hdx_put_u16_be(msg.Data, 4U, g_stCellInfoReport.SocElement.u16CapacityNow);
	hdx_fill_crc(&msg);
	return Can_HDX_Transmit(&msg);
}

static UINT8 hdx_send_frame_01(void)
{
	CanTxMsg msg;

	hdx_init_frame(&msg, 0x01U);
	hdx_put_u16_be(msg.Data, 0U, g_stCellInfoReport.SocElement.u16CapacityFull);
	hdx_put_u16_be(msg.Data, 2U, g_stCellInfoReport.SocElement.u16Cycle_times);
	hdx_put_u16_be(msg.Data, 4U, g_stCellInfoReport.SocElement.u16Soc);
	hdx_fill_crc(&msg);
	return Can_HDX_Transmit(&msg);
}

static UINT8 hdx_send_frame_02(void)
{
	CanTxMsg msg;

	hdx_init_frame(&msg, 0x02U);
	hdx_put_u16_be(msg.Data, 0U, g_stCellInfoReport.u16BalanceFlag1);
	hdx_put_u16_be(msg.Data, 2U, g_stCellInfoReport.u16BalanceFlag2);
	hdx_put_u16_be(msg.Data, 4U, g_stCellInfoReport.unMdlFault_Third.all);
	hdx_fill_crc(&msg);
	return Can_HDX_Transmit(&msg);
}

static UINT8 hdx_send_frame_03(void)
{
	CanTxMsg msg;
	UINT16 status;

	hdx_init_frame(&msg, 0x03U);
	status = (UINT16)(SystemRuntime_GetStatusSnapshot() & 0x000003FFUL);
	hdx_put_u16_be(msg.Data, 0U, status);
	hdx_put_u16_be(msg.Data, 2U, 0x1234U);
	hdx_put_u16_be(msg.Data, 4U, 0x1234U);
	hdx_fill_crc(&msg);
	return Can_HDX_Transmit(&msg);
}

static UINT8 hdx_send_frame_04(void)
{
	CanTxMsg msg;

	hdx_init_frame(&msg, 0x04U);
	msg.Data[0] = SeriesNum;
	msg.Data[1] = (System_ErrFlag.u8ErrFlag_CBC_DSG > 0U) ? 1U : 0U;
	hdx_put_u16_be(msg.Data, 2U, g_stCellInfoReport.u16TempMax);
	hdx_put_u16_be(msg.Data, 4U, g_stCellInfoReport.u16TempMin);
	hdx_fill_crc(&msg);
	return Can_HDX_Transmit(&msg);
}

static UINT8 hdx_send_frame_05(void)
{
	CanTxMsg msg;

	hdx_init_frame(&msg, 0x05U);
	hdx_put_u16_be(msg.Data, 0U, g_stCellInfoReport.SocElement.u16Soh);
	hdx_put_u16_be(msg.Data, 2U, 0U);
	hdx_put_u16_be(msg.Data, 4U, 0U);
	hdx_fill_crc(&msg);
	return Can_HDX_Transmit(&msg);
}

static UINT8 hdx_send_frame_06(void)
{
	CanTxMsg msg;

	hdx_init_frame(&msg, 0x06U);
	hdx_put_u16_be(msg.Data, 0U, 0U);
	hdx_put_u16_be(msg.Data, 2U, 0U);
	hdx_put_u16_be(msg.Data, 4U, 0U);
	hdx_fill_crc(&msg);
	return Can_HDX_Transmit(&msg);
}

static UINT8 hdx_send_cell_frame(UINT8 frame_id, UINT8 first_cell)
{
	CanTxMsg msg;
	UINT8 i;
	UINT8 cell_index;
	UINT16 value;

	hdx_init_frame(&msg, frame_id);
	for (i = 0U; i < 3U; ++i)
	{
		cell_index = (UINT8)(first_cell + i);
		if (cell_index < 32U)
		{
			value = g_stCellInfoReport.u16VCell[cell_index];
		}
		else
		{
			value = 0U;
		}
		hdx_put_u16_be(msg.Data, (UINT8)(i * 2U), value);
	}
	hdx_fill_crc(&msg);
	return Can_HDX_Transmit(&msg);
}

static UINT8 hdx_send_frame(UINT8 frame_id)
{
	switch (frame_id)
	{
	case 0x00U:
		return hdx_send_frame_00();
	case 0x01U:
		return hdx_send_frame_01();
	case 0x02U:
		return hdx_send_frame_02();
	case 0x03U:
		return hdx_send_frame_03();
	case 0x04U:
		return hdx_send_frame_04();
	case 0x05U:
		return hdx_send_frame_05();
	case 0x06U:
		return hdx_send_frame_06();
	case 0x07U:
		return hdx_send_cell_frame(0x07U, 0U);
	case 0x08U:
		return hdx_send_cell_frame(0x08U, 3U);
	case 0x09U:
		return hdx_send_cell_frame(0x09U, 6U);
	case 0x0AU:
		return hdx_send_cell_frame(0x0AU, 9U);
	case 0x0BU:
		return hdx_send_cell_frame(0x0BU, 12U);
	case 0x0CU:
		return hdx_send_cell_frame(0x0CU, 15U);
	case 0x0DU:
		return hdx_send_cell_frame(0x0DU, 18U);
	case 0x0EU:
		return hdx_send_cell_frame(0x0EU, 21U);
	case 0x0FU:
		return hdx_send_cell_frame(0x0FU, 24U);
	case 0x10U:
		return hdx_send_cell_frame(0x10U, 27U);
	case 0x11U:
		return hdx_send_cell_frame(0x11U, 30U);
	default:
		return CAN_TxStatus_NoMailBox;
	}
}

static UINT8 hdx_queue_request(UINT16 std_id)
{
	UINT8 frame_id;

	if ((std_id >> 7) != (UINT16)CAN_ADRESS_STD_ID)
	{
		return 0U;
	}

	frame_id = (UINT8)(std_id & 0x007FU);
	if (frame_id > HDX_CAN_FRAME_ID_MAX)
	{
		return 0U;
	}

	s_hdx_pending_mask |= (1UL << frame_id);
	return 1U;
}

static UINT8 hdx_has_pending_request(void)
{
	return (s_hdx_pending_mask != 0U) ? 1U : 0U;
}

static void hdx_clear_pending_requests(void)
{
	UINT32 primask = __get_PRIMASK();

	__disable_irq();
	s_hdx_pending_mask = 0U;
	if (primask == 0U)
	{
		__enable_irq();
	}
}

static void hdx_process_next_request(void)
{
	UINT32 primask = __get_PRIMASK();
	UINT32 pending;
	UINT8 frame_id;
	UINT8 found = 0U;

	__disable_irq();
	pending = s_hdx_pending_mask;
	for (frame_id = HDX_CAN_FRAME_ID_MIN; frame_id <= HDX_CAN_FRAME_ID_MAX; ++frame_id)
	{
		if ((pending & (1UL << frame_id)) != 0U)
		{
			s_hdx_pending_mask &= ~(1UL << frame_id);
			found = 1U;
			break;
		}
	}
	if (primask == 0U)
	{
		__enable_irq();
	}

	if (found != 0U)
	{
		(void)hdx_send_frame(frame_id);
	}
}

static UINT8 comm_tool_crc_ok(const UINT8 data[8])
{
	UINT16 expect_crc = (UINT16)(((UINT16)data[6] << 8) | data[7]);
	UINT16 actual_crc = Sci_CRC16RTU((UINT8 *)data, 6U);

	return (expect_crc == actual_crc) ? 1U : 0U;
}

static void comm_tool_fill_crc(UINT8 data[8])
{
	UINT16 crc = Sci_CRC16RTU(data, 6U);
	data[6] = (UINT8)(crc >> 8);
	data[7] = (UINT8)crc;
}

static void comm_tool_send_frame(UINT8 cmd, UINT8 status_or_seq, UINT8 value0, UINT8 value1)
{
	CanTxMsg tx_msg;

	memset(&tx_msg, 0, sizeof(tx_msg));
	tx_msg.StdId = COMM_TOOL_CAN_ACK_ID;
	tx_msg.IDE = CAN_ID_STD;
	tx_msg.RTR = CAN_RTR_DATA;
	tx_msg.DLC = 8U;
	tx_msg.Data[0] = 0x5AU;
	tx_msg.Data[1] = 0xA5U;
	tx_msg.Data[2] = cmd;
	tx_msg.Data[3] = status_or_seq;
	tx_msg.Data[4] = value0;
	tx_msg.Data[5] = value1;
	comm_tool_fill_crc(tx_msg.Data);
	(void)Can_HDX_Transmit(&tx_msg);
}

static UINT8 comm_tool_status_from_host_error(UINT8 error)
{
	switch (error)
	{
	case 0U:
		return COMM_TOOL_CAN_ACK_OK;
	case RS485_ERROR_NO_PERMISSION:
		return COMM_TOOL_CAN_ACK_NO_PERMISSION;
	case RS485_ERROR_ADDR_INVALID:
	case RS485_ERROR_DATA_INVALID:
	case RS485_ERROR_RONLY_NO_W:
	case RS485_ERROR_WONLY_NO_R:
		return COMM_TOOL_CAN_ACK_BAD_PARAM;
	default:
		return COMM_TOOL_CAN_ACK_BMS_ERROR;
	}
}

static void comm_tool_clear_cmd_queue(void)
{
	UINT32 primask = __get_PRIMASK();

	__disable_irq();
	s_comm_tool.cmd_head = 0U;
	s_comm_tool.cmd_tail = 0U;
	s_comm_tool.cmd_count = 0U;
	if (primask == 0U)
	{
		__enable_irq();
	}
}

static UINT8 comm_tool_take_cmd(UINT8 data[8])
{
	UINT32 primask = __get_PRIMASK();
	UINT8 has_cmd = 0U;

	__disable_irq();
	if (s_comm_tool.cmd_count != 0U)
	{
		memcpy(data, s_comm_tool.cmd_queue[s_comm_tool.cmd_head], 8U);
		s_comm_tool.cmd_head++;
		if (s_comm_tool.cmd_head >= COMM_TOOL_CAN_CMD_QUEUE_SIZE)
		{
			s_comm_tool.cmd_head = 0U;
		}
		s_comm_tool.cmd_count--;
		has_cmd = 1U;
	}
	if (primask == 0U)
	{
		__enable_irq();
	}

	return has_cmd;
}

static void comm_tool_queue_cmd(const UINT8 data[8])
{
	if (s_comm_tool.cmd_count >= COMM_TOOL_CAN_CMD_QUEUE_SIZE)
	{
		return;
	}

	memcpy(s_comm_tool.cmd_queue[s_comm_tool.cmd_tail], data, 8U);
	s_comm_tool.cmd_tail++;
	if (s_comm_tool.cmd_tail >= COMM_TOOL_CAN_CMD_QUEUE_SIZE)
	{
		s_comm_tool.cmd_tail = 0U;
	}
	s_comm_tool.cmd_count++;
}

static void comm_tool_start_read_block_stream(UINT8 count)
{
	s_comm_tool.read_block_count = count;
	s_comm_tool.read_block_index = 0U;
	s_comm_tool.read_block_active = 1U;
	s_comm_tool.read_block_last_tick = s_runtime.tick - COMM_TOOL_CAN_READ_BLOCK_FRAME_INTERVAL_TICKS;
}

static void comm_tool_stop_read_block_stream(void)
{
	s_comm_tool.read_block_active = 0U;
	s_comm_tool.read_block_count = 0U;
	s_comm_tool.read_block_index = 0U;
}

static void comm_tool_service_read_block_stream(UINT32 now_tick)
{
	if (s_comm_tool.read_block_active == 0U)
	{
		return;
	}
	if (s_comm_tool.read_block_index >= s_comm_tool.read_block_count)
	{
		comm_tool_stop_read_block_stream();
		return;
	}
	if (s_tx.count > (CAN_TX_QUEUE_SIZE - 4U))
	{
		return;
	}
	if (can_tick_elapsed(now_tick,
						s_comm_tool.read_block_last_tick,
						COMM_TOOL_CAN_READ_BLOCK_FRAME_INTERVAL_TICKS) == 0U)
	{
		return;
	}

	comm_tool_send_frame(COMM_TOOL_CAN_CMD_READ_BLOCK_DATA,
					 s_comm_tool.read_block_index,
					 (UINT8)(s_comm_tool.read_block_words[s_comm_tool.read_block_index] >> 8),
					 (UINT8)s_comm_tool.read_block_words[s_comm_tool.read_block_index]);
	s_comm_tool.read_block_index++;
	s_comm_tool.read_block_last_tick = now_tick;
	if (s_comm_tool.read_block_index >= s_comm_tool.read_block_count)
	{
		comm_tool_stop_read_block_stream();
	}
}

static void comm_tool_handle_cmd_data(const UINT8 data[8])
{
	UINT8 status = COMM_TOOL_CAN_ACK_OK;
	UINT8 value0 = 0U;
	UINT8 value1 = 0U;
	UINT8 cmd;
	UINT16 reg_addr;
	UINT16 reg_value;
	UINT8 reg_count;
	UINT8 host_error;

	if ((data[0] != 0xA5U) ||
		(data[1] != 0x5AU) ||
		(comm_tool_crc_ok(data) == 0U))
	{
		return;
	}

	cmd = data[2];
	comm_tool_stop_read_block_stream();

	switch (cmd)
	{
	case COMM_TOOL_CAN_CMD_GET_STATUS:
		reg_value = g_stCellInfoReport.SocElement.u16Soc;
		value0 = (UINT8)((reg_value > 100U) ? 100U : reg_value);
		reg_value = g_stCellInfoReport.SocElement.u16Soh;
		value1 = (UINT8)((reg_value > 100U) ? 100U : reg_value);
		break;

	case COMM_TOOL_CAN_CMD_ENTER_IAP:
		if ((data[3] != 0xC3U) ||
			(data[4] != 0x3CU) ||
			(data[5] != (UINT8)CAN_ADRESS_STD_ID))
		{
			status = COMM_TOOL_CAN_ACK_BAD_PARAM;
			break;
		}
		if (AppUpgrade_RequestIap() == 0U)
		{
			status = COMM_TOOL_CAN_ACK_FLASH_ERR;
			break;
		}
		value0 = 0x08U;
		value1 = 0x48U;
		s_comm_tool.enter_iap_delay_ticks = COMM_TOOL_CAN_ENTER_IAP_DELAY_TICKS;
		break;

	case COMM_TOOL_CAN_CMD_READ_REG:
		reg_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		host_error = Sci_HostReadWords(reg_addr, 1U, &reg_value);
		status = comm_tool_status_from_host_error(host_error);
		if (status == COMM_TOOL_CAN_ACK_OK)
		{
			value0 = (UINT8)(reg_value >> 8);
			value1 = (UINT8)reg_value;
		}
		break;

	case COMM_TOOL_CAN_CMD_READ_BLOCK:
		reg_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		reg_count = data[5];
		if ((reg_count == 0U) ||
			(reg_count > COMM_TOOL_CAN_READ_BLOCK_MAX_WORDS) ||
			(((UINT32)reg_addr + (UINT32)reg_count - 1U) > (UINT32)0xFFFFU))
		{
			status = COMM_TOOL_CAN_ACK_BAD_PARAM;
			break;
		}
		host_error = Sci_HostReadWords(reg_addr, reg_count, s_comm_tool.read_block_words);
		status = comm_tool_status_from_host_error(host_error);
		if (status == COMM_TOOL_CAN_ACK_OK)
		{
			value0 = reg_count;
			value1 = 0U;
			comm_tool_start_read_block_stream(reg_count);
		}
		break;

	case COMM_TOOL_CAN_CMD_WRITE_PREP:
		s_comm_tool.write_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		s_comm_tool.write_value_hi = data[5];
		s_comm_tool.write_pending = 1U;
		value0 = data[3];
		value1 = data[4];
		break;

	case COMM_TOOL_CAN_CMD_WRITE_COMMIT:
		reg_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		if ((s_comm_tool.write_pending == 0U) || (reg_addr != s_comm_tool.write_addr))
		{
			s_comm_tool.write_pending = 0U;
			status = COMM_TOOL_CAN_ACK_BAD_PARAM;
			break;
		}
		reg_value = (UINT16)(((UINT16)s_comm_tool.write_value_hi << 8) | data[5]);
		s_comm_tool.write_pending = 0U;
		host_error = Sci_HostWriteWords(reg_addr, &reg_value, 1U);
		status = comm_tool_status_from_host_error(host_error);
		break;

	default:
		status = COMM_TOOL_CAN_ACK_BAD_CMD;
		break;
	}

	comm_tool_send_frame(cmd, status, value0, value1);
}

static void comm_tool_service_enter_iap_delay(void)
{
	if ((s_comm_tool.enter_iap_delay_ticks == 0U) ||
		(0 == g_st_SysTimeFlag.bits.b1Sys10msFlag))
	{
		return;
	}

	s_comm_tool.enter_iap_delay_ticks--;
	if (s_comm_tool.enter_iap_delay_ticks == 0U)
	{
		u8FlashUpdateFlag = 1U;
	}
}

static void can_handle_rx_msg(const CanRxMsg *rx_msg)
{
	UINT16 comm_tool_cmd_id;

	if ((rx_msg == 0) || (rx_msg->IDE != CAN_ID_STD))
	{
		return;
	}

	comm_tool_cmd_id = (UINT16)(((UINT16)CAN_ADRESS_STD_ID << 7) | COMM_TOOL_CAN_CMD_ID);
	if (((UINT16)rx_msg->StdId == comm_tool_cmd_id) && (rx_msg->DLC == 8U))
	{
		comm_tool_queue_cmd(rx_msg->Data);
		return;
	}

	(void)hdx_queue_request((UINT16)rx_msg->StdId);
}

static void InitCan_GPIO(void)
{
	GPIO_InitTypeDef gpio;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

	GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, CAN_POWER_ON_LEVEL);
	gpio.GPIO_Pin = PIN_CMNT_EN;
	gpio.GPIO_Mode = GPIO_Mode_Out_PP;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIO_CMNT_EN, &gpio);
	can_power_on();

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
	can_mark_tx_idle();
	s_runtime.tick = 0U;
	s_comm_tool.write_pending = 0U;
	s_comm_tool.enter_iap_delay_ticks = 0U;
	comm_tool_stop_read_block_stream();
	can_clear_tx_queue();
	comm_tool_clear_cmd_queue();
	hdx_clear_pending_requests();
	InitCan_GPIO();
	InitCan_NVIC();
	InitCan_CAN1();
	InitCan_Filter();
	can_power_on();
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
	if (s_comm_tool.read_block_active != 0U)
	{
		return 1U;
	}
	if (s_comm_tool.cmd_count != 0U)
	{
		return 1U;
	}
	if (hdx_has_pending_request() != 0U)
	{
		return 1U;
	}
	return ((CAN1->TSR & CAN_TSR_TME) != CAN_TSR_TME) ? 1U : 0U;
}

static UINT8 can_has_sleep_blocking_work(void)
{
	if (can_queue_has_request() != 0U)
	{
		return 1U;
	}
	if ((s_tx.mailbox != CAN_TxStatus_NoMailBox) &&
		(s_tx.mailbox_source != CAN_TX_SOURCE_PERIODIC))
	{
		return 1U;
	}
	if (s_comm_tool.read_block_active != 0U)
	{
		return 1U;
	}
	if (s_comm_tool.cmd_count != 0U)
	{
		return 1U;
	}
	if (s_comm_tool.enter_iap_delay_ticks != 0U)
	{
		return 1U;
	}
	if (hdx_has_pending_request() != 0U)
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
	if (sys_time.last_ext_comm_cnt_can != sys_time.can_rcv_cnt)
	{
		sys_time.last_ext_comm_cnt_can = sys_time.can_rcv_cnt;
		return 1U;
	}
	return 0U;
}

void Can_PrepareSleep(void)
{
	can_abort_tx();
	comm_tool_clear_cmd_queue();
	comm_tool_stop_read_block_stream();
	s_comm_tool.write_pending = 0U;
	hdx_clear_pending_requests();
	can_power_off();
}

void App_Can(void)
{
	UINT32 now_tick = SysTime_Get10msTickCount();
	UINT8 app_cmd_data[8];

	s_runtime.tick = now_tick;
	if (comm_tool_take_cmd(app_cmd_data) != 0U)
	{
		comm_tool_handle_cmd_data(app_cmd_data);
	}
	hdx_process_next_request();
	comm_tool_service_read_block_stream(now_tick);
	can_service_tx(now_tick);
	comm_tool_service_enter_iap_delay();
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
	CanRxMsg rx_msg;

	while (CAN_MessagePending(CAN1, CAN_FIFO0) != 0U)
	{
		sys_time.can_rcv_cnt++;
		CAN_Receive(CAN1, CAN_FIFO0, &rx_msg);
		can_handle_rx_msg(&rx_msg);
	}
}
