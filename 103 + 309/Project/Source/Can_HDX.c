#include "main.h"
#include "CanFeidaoFrames.h"
#include "FactoryAging.h"
#include <string.h>

#if PROJECT_CFG_DEBUG_WATCH_ENABLE
volatile struct CAN_ERROR_SNAPSHOT g_stCanErrorSnapshot;
volatile struct CAN_LOW_POWER_STATUS g_stCanLowPowerStatus;
#define FEIDAO_CAN_ERROR_INC(field) feidao_can_inc_u16(&g_stCanErrorSnapshot.field)
#else
#define FEIDAO_CAN_ERROR_INC(field) do { } while (0)
#endif

UINT16 g_u16BusOff_InitTestCnt = 0U;
UINT16 g_u16BusOff_RecoverCnt = 0U;

#define FEIDAO_CAN_POWER_ON_LEVEL Bit_RESET
#define FEIDAO_CAN_POWER_OFF_LEVEL Bit_SET

#define FEIDAO_CAN_PERIOD_1000MS_TICKS ((UINT32)100U)
#define FEIDAO_CAN_PERIOD_5000MS_TICKS ((UINT32)500U)
#define FEIDAO_CAN_TX_TIMEOUT_TICKS ((UINT32)20U)
#define FEIDAO_CAN_RTC_PERIOD_SECONDS ((UINT32)1U)
#define FEIDAO_CAN_RTC_SERVICE_TIMEOUT_TICKS ((UINT32)150U)

#define FEIDAO_CAN_TX_QUEUE_SIZE ((UINT8)32U)
#define FEIDAO_CAN_APP_CMD_QUEUE_SIZE ((UINT8)4U)

#define FEIDAO_CAN_APP_CMD_ID ((UINT16)0x60U)
#define FEIDAO_CAN_APP_ACK_ID ((UINT16)0x61U)
#define FEIDAO_CAN_APP_CMD_GET_STATUS ((UINT8)0x01U)
#define FEIDAO_CAN_APP_CMD_ENTER_IAP ((UINT8)0x02U)
#define FEIDAO_CAN_APP_CMD_READ_REG ((UINT8)0x03U)
#define FEIDAO_CAN_APP_CMD_WRITE_PREP ((UINT8)0x04U)
#define FEIDAO_CAN_APP_CMD_WRITE_COMMIT ((UINT8)0x05U)
#define FEIDAO_CAN_APP_CMD_READ_BLOCK ((UINT8)0x06U)
#define FEIDAO_CAN_APP_CMD_AGING_START ((UINT8)0x07U)
#define FEIDAO_CAN_APP_CMD_AGING_STOP ((UINT8)0x08U)
#define FEIDAO_CAN_APP_CMD_AGING_RESET_TIME ((UINT8)0x09U)
#define FEIDAO_CAN_APP_CMD_AGING_SET_HOURS ((UINT8)0x0AU)
#define FEIDAO_CAN_APP_CMD_READ_BLOCK_DATA ((UINT8)0x86U)
#define FEIDAO_CAN_APP_READ_BLOCK_MAX_WORDS ((UINT8)120U)
#define FEIDAO_CAN_APP_READ_BLOCK_FRAME_INTERVAL_TICKS ((UINT32)1U)
#define FEIDAO_CAN_APP_AGING_GUARD ((UINT8)0xA9U)
#define FEIDAO_CAN_APP_AGING_ACTION_START ((UINT8)0x51U)
#define FEIDAO_CAN_APP_AGING_ACTION_STOP ((UINT8)0x50U)
#define FEIDAO_CAN_APP_AGING_ACTION_RESET_TIME ((UINT8)0x5AU)
#define FEIDAO_CAN_APP_ACK_OK ((UINT8)0x00U)
#define FEIDAO_CAN_APP_ACK_BAD_CMD ((UINT8)0x01U)
#define FEIDAO_CAN_APP_ACK_BAD_PARAM ((UINT8)0x02U)
#define FEIDAO_CAN_APP_ACK_FLASH_ERR ((UINT8)0x05U)
#define FEIDAO_CAN_APP_ACK_NO_PERMISSION ((UINT8)0x07U)
#define FEIDAO_CAN_APP_ACK_BMS_ERROR ((UINT8)0x08U)
#define FEIDAO_CAN_APP_ENTER_IAP_DELAY_TICKS ((UINT8)20U)

#define FEIDAO_CAN_TME_FLAG(mailbox) ((UINT32)(CAN_TSR_TME0 << (mailbox)))
#define FEIDAO_CAN_RQCP_FLAG(mailbox) ((UINT32)(0x38000000U | (1UL << ((mailbox) * 8U))))

typedef struct
{
	CanTxMsg frame;
} FeidaoCanTxItem;

static FeidaoCanTxItem s_stTxQueue[FEIDAO_CAN_TX_QUEUE_SIZE];
static UINT8 s_u8TxQueueHead = 0U;
static UINT8 s_u8TxQueueTail = 0U;
static UINT8 s_u8TxQueueCount = 0U;
static UINT8 s_u8TxMailbox = CAN_TxStatus_NoMailBox;
static UINT32 s_u32TxStartTick = 0U;

static UINT32 s_u32CanTick = 0U;
static UINT32 s_u32Last1000msTick = 0U;
static UINT32 s_u32Last5000msTick = 0U;
static UINT8 s_u8ScheduleInit = 0U;
static UINT8 s_u8BusActive = 0U;
static UINT8 s_u8CanBusOff = 0U;
static volatile UINT8 s_u8RtcServiceActive = 0U;
static volatile UINT8 s_u8LastRtcWakeTimeout = 0U;
static UINT16 s_u16RtcWakeServiceCnt = 0U;
static UINT16 s_u16PrepareSleepCnt = 0U;

static volatile UINT8 s_u8AppCmdQueueHead = 0U;
static volatile UINT8 s_u8AppCmdQueueTail = 0U;
static volatile UINT8 s_u8AppCmdQueueCount = 0U;
static UINT8 s_u8AppCmdQueue[FEIDAO_CAN_APP_CMD_QUEUE_SIZE][8];
static UINT8 s_u8WritePending = 0U;
static UINT16 s_u16WriteAddr = 0U;
static UINT8 s_u8WriteValueHi = 0U;
static UINT8 s_u8EnterIapDelayTicks = 0U;

static UINT16 s_u16ReadBlockWords[FEIDAO_CAN_APP_READ_BLOCK_MAX_WORDS];
static UINT8 s_u8ReadBlockCount = 0U;
static UINT8 s_u8ReadBlockIndex = 0U;
static UINT8 s_u8ReadBlockActive = 0U;
static UINT32 s_u32ReadBlockLastTick = 0U;

static void feidao_can_inc_u16(volatile UINT16 *counter);
static UINT8 feidao_can_tick_elapsed(UINT32 now_tick, UINT32 start_tick, UINT32 wait_ticks);
static void feidao_can_update_error_snapshot(void);
static void feidao_can_record_ack_error(void);
static void feidao_can_record_tx_failed(void);
static void feidao_can_record_tx_timeout(void);
static void feidao_can_record_tx_no_mailbox(void);
static void feidao_can_record_tx_abort(void);
static void feidao_can_update_debug_status(void);
static void feidao_can_clear_tx_done(UINT8 mailbox);
static void feidao_can_cancel_tx(UINT8 mailbox);
static UINT8 feidao_can_enqueue_tx(const CanTxMsg *frame);
static UINT8 feidao_can_dequeue_tx(CanTxMsg *frame);
static void feidao_can_clear_tx_queue(void);
static void feidao_can_service_tx(UINT32 now_tick);
static void feidao_can_queue_periodic_mask(UINT16 mask);
static void feidao_can_schedule_periodic(UINT32 now_tick);
static void feidao_can_busoff_monitor(void);
static UINT8 feidao_can_app_crc_ok(const UINT8 data[8]);
static void feidao_can_app_fill_crc(UINT8 data[8]);
static UINT8 feidao_can_u16_to_percent(UINT16 value);
static UINT8 feidao_can_aging_guard_ok(const UINT8 data[8], UINT8 action);
static UINT8 feidao_can_aging_remaining_hours(void);
static void feidao_can_fill_aging_ack(UINT8 *value0, UINT8 *value1);
static void feidao_can_app_send_ack(UINT8 cmd, UINT8 status, UINT8 value0, UINT8 value1);
static void feidao_can_app_send_word_frame(UINT8 seq, UINT16 value);
static UINT8 feidao_can_app_status_from_host_error(UINT8 error);
static void feidao_can_clear_app_cmd_queue(void);
static UINT8 feidao_can_take_app_cmd(UINT8 data[8]);
static void feidao_can_queue_app_cmd(const UINT8 data[8]);
static void feidao_can_handle_app_cmd_data(const UINT8 data[8]);
static void feidao_can_service_app_cmd(void);
static void feidao_can_start_read_block_stream(UINT8 count);
static void feidao_can_stop_read_block_stream(void);
static void feidao_can_service_read_block_stream(UINT32 now_tick);
static void feidao_can_service_enter_iap_delay(void);
static void feidao_can_handle_rx_msg(const CanRxMsg *rx_msg);
static void InitCan_GPIO(void);
static void InitCan_NVIC(void);
static void InitCan_Filter(void);
static void InitCan_CAN1(void);

static void feidao_can_inc_u16(volatile UINT16 *counter)
{
	if (*counter < (UINT16)0xFFFFU)
	{
		(*counter)++;
	}
}

static UINT8 feidao_can_tick_elapsed(UINT32 now_tick, UINT32 start_tick, UINT32 wait_ticks)
{
	return (((UINT32)(now_tick - start_tick)) >= wait_ticks) ? 1U : 0U;
}

static void feidao_can_update_error_snapshot(void)
{
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
	UINT32 esr = CAN1->ESR;
	g_stCanErrorSnapshot.u8LastErrorCode = (UINT8)(esr & CAN_ESR_LEC);
	g_stCanErrorSnapshot.u8ReceiveErrorCounter = (UINT8)((esr & CAN_ESR_REC) >> 24);
	g_stCanErrorSnapshot.u8TransmitErrorCounter = (UINT8)((esr & CAN_ESR_TEC) >> 16);
	g_stCanErrorSnapshot.u8ErrorWarning = (UINT8)((esr & CAN_ESR_EWGF) ? 1U : 0U);
	g_stCanErrorSnapshot.u8ErrorPassive = (UINT8)((esr & CAN_ESR_EPVF) ? 1U : 0U);
	g_stCanErrorSnapshot.u8BusOff = (UINT8)((esr & CAN_ESR_BOFF) ? 1U : 0U);
#endif
}

static void feidao_can_record_ack_error(void)
{
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
	if (CAN_ErrorCode_ACKErr == g_stCanErrorSnapshot.u8LastErrorCode)
	{
		FEIDAO_CAN_ERROR_INC(u16AckErrorCnt);
	}
#endif
}

static void feidao_can_record_tx_failed(void)
{
	feidao_can_update_error_snapshot();
	FEIDAO_CAN_ERROR_INC(u16TxFailedCnt);
	feidao_can_record_ack_error();
}

static void feidao_can_record_tx_timeout(void)
{
	feidao_can_update_error_snapshot();
	FEIDAO_CAN_ERROR_INC(u16TxTimeoutCnt);
}

static void feidao_can_record_tx_no_mailbox(void)
{
	feidao_can_update_error_snapshot();
	FEIDAO_CAN_ERROR_INC(u16TxNoMailboxCnt);
}

static void feidao_can_record_tx_abort(void)
{
	FEIDAO_CAN_ERROR_INC(u16TxAbortCnt);
}

static void feidao_can_update_debug_status(void)
{
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
	g_stCanLowPowerStatus.u8PowerState = 1U;
	g_stCanLowPowerStatus.u8BusActive = s_u8BusActive;
	g_stCanLowPowerStatus.u8NoAckCnt = 0U;
	g_stCanLowPowerStatus.u8ProbeActive = 0U;
	g_stCanLowPowerStatus.u8TxMailbox = s_u8TxMailbox;
	g_stCanLowPowerStatus.u8RtcServiceActive = s_u8RtcServiceActive;
	g_stCanLowPowerStatus.u8LastRtcWakeTxAcked = s_u8BusActive;
	g_stCanLowPowerStatus.u8LastRtcWakeTimeout = s_u8LastRtcWakeTimeout;
	g_stCanLowPowerStatus.u16PendingMask = s_u8TxQueueCount;
	g_stCanLowPowerStatus.u16RtcWakeServiceCnt = s_u16RtcWakeServiceCnt;
	g_stCanLowPowerStatus.u16PrepareSleepCnt = s_u16PrepareSleepCnt;
	g_stCanLowPowerStatus.u32LogicalTick = s_u32CanTick;
	g_stCanLowPowerStatus.u32LastRtcElapsedSeconds = 0U;
#else
	(void)s_u8RtcServiceActive;
	(void)s_u8LastRtcWakeTimeout;
#endif
}

static void feidao_can_clear_tx_done(UINT8 mailbox)
{
	if (mailbox < 3U)
	{
		CAN_ClearFlag(CAN1, FEIDAO_CAN_RQCP_FLAG(mailbox));
	}
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
	feidao_can_record_tx_abort();
}

static UINT8 feidao_can_enqueue_tx(const CanTxMsg *frame)
{
	if ((frame == 0) || (frame->DLC > 8U) || (s_u8TxQueueCount >= FEIDAO_CAN_TX_QUEUE_SIZE))
	{
		feidao_can_record_tx_no_mailbox();
		return 0U;
	}

	s_stTxQueue[s_u8TxQueueTail].frame = *frame;
	s_u8TxQueueTail++;
	if (s_u8TxQueueTail >= FEIDAO_CAN_TX_QUEUE_SIZE)
	{
		s_u8TxQueueTail = 0U;
	}
	s_u8TxQueueCount++;
	return 1U;
}

static UINT8 feidao_can_dequeue_tx(CanTxMsg *frame)
{
	if ((frame == 0) || (s_u8TxQueueCount == 0U))
	{
		return 0U;
	}

	*frame = s_stTxQueue[s_u8TxQueueHead].frame;
	s_u8TxQueueHead++;
	if (s_u8TxQueueHead >= FEIDAO_CAN_TX_QUEUE_SIZE)
	{
		s_u8TxQueueHead = 0U;
	}
	s_u8TxQueueCount--;
	return 1U;
}

static void feidao_can_clear_tx_queue(void)
{
	s_u8TxQueueHead = 0U;
	s_u8TxQueueTail = 0U;
	s_u8TxQueueCount = 0U;
}

static void feidao_can_service_tx(UINT32 now_tick)
{
	CanTxMsg frame;
	UINT8 status;

	if (s_u8TxMailbox != CAN_TxStatus_NoMailBox)
	{
		status = CAN_TransmitStatus(CAN1, s_u8TxMailbox);
		if (status == CAN_TxStatus_Ok)
		{
			s_u8BusActive = 1U;
			feidao_can_clear_tx_done(s_u8TxMailbox);
			s_u8TxMailbox = CAN_TxStatus_NoMailBox;
		}
		else if (status == CAN_TxStatus_Failed)
		{
			feidao_can_record_tx_failed();
			feidao_can_clear_tx_done(s_u8TxMailbox);
			s_u8TxMailbox = CAN_TxStatus_NoMailBox;
		}
		else if (feidao_can_tick_elapsed(now_tick, s_u32TxStartTick, FEIDAO_CAN_TX_TIMEOUT_TICKS))
		{
			feidao_can_record_tx_timeout();
			feidao_can_cancel_tx(s_u8TxMailbox);
			s_u8TxMailbox = CAN_TxStatus_NoMailBox;
		}
	}

	if ((s_u8TxMailbox == CAN_TxStatus_NoMailBox) && (s_u8CanBusOff == 0U) && feidao_can_dequeue_tx(&frame))
	{
		s_u8TxMailbox = CAN_Transmit(CAN1, &frame);
		if (s_u8TxMailbox == CAN_TxStatus_NoMailBox)
		{
			feidao_can_record_tx_no_mailbox();
		}
		else
		{
			s_u32TxStartTick = now_tick;
		}
	}
}

UINT8 Can_HDX_Transmit(CanTxMsg *Msg)
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

	return feidao_can_enqueue_tx(&frame) ? 0U : CAN_TxStatus_NoMailBox;
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
	if (s_u8ScheduleInit == 0U)
	{
		s_u8ScheduleInit = 1U;
		s_u32Last1000msTick = now_tick;
		s_u32Last5000msTick = now_tick;
		return;
	}

	if (feidao_can_tick_elapsed(now_tick, s_u32Last1000msTick, FEIDAO_CAN_PERIOD_1000MS_TICKS))
	{
		s_u32Last1000msTick = now_tick;
		feidao_can_queue_periodic_mask(CAN_FEIDAO_1000MS_MSG_MASK);
	}
	if (feidao_can_tick_elapsed(now_tick, s_u32Last5000msTick, FEIDAO_CAN_PERIOD_5000MS_TICKS))
	{
		s_u32Last5000msTick = now_tick;
		feidao_can_queue_periodic_mask(CAN_FEIDAO_5000MS_MSG_MASK);
	}
}

static void feidao_can_busoff_monitor(void)
{
	UINT8 bus_off = ((CAN1->ESR & CAN_ESR_BOFF) != 0U) ? 1U : 0U;

	if ((bus_off != 0U) && (s_u8CanBusOff == 0U))
	{
		s_u8CanBusOff = 1U;
		g_u16BusOff_InitTestCnt++;
		feidao_can_update_error_snapshot();
		FEIDAO_CAN_ERROR_INC(u16BusOffCnt);
		if (s_u8TxMailbox != CAN_TxStatus_NoMailBox)
		{
			feidao_can_cancel_tx(s_u8TxMailbox);
			s_u8TxMailbox = CAN_TxStatus_NoMailBox;
		}
		feidao_can_clear_tx_queue();
	}
	else if ((bus_off == 0U) && (s_u8CanBusOff != 0U))
	{
		s_u8CanBusOff = 0U;
		g_u16BusOff_RecoverCnt++;
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

static UINT8 feidao_can_u16_to_percent(UINT16 value)
{
	return (value > 100U) ? 100U : (UINT8)value;
}

static UINT8 feidao_can_aging_guard_ok(const UINT8 data[8], UINT8 action)
{
	return ((data[3] == FEIDAO_CAN_APP_AGING_GUARD) &&
			(data[4] == action) &&
			(data[5] == (UINT8)CAN_ADRESS_STD_ID)) ? 1U : 0U;
}

static UINT8 feidao_can_aging_remaining_hours(void)
{
	UINT32 hours = (FactoryAging_GetRemainingSeconds() + 3599U) / 3600U;

	return (hours > 0xFFU) ? 0xFFU : (UINT8)hours;
}

static void feidao_can_fill_aging_ack(UINT8 *value0, UINT8 *value1)
{
	if (value0 != 0)
	{
		*value0 = FactoryAging_GetState();
	}
	if (value1 != 0)
	{
		*value1 = feidao_can_aging_remaining_hours();
	}
}

static void feidao_can_app_send_ack(UINT8 cmd, UINT8 status, UINT8 value0, UINT8 value1)
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
	tx_msg.Data[3] = status;
	tx_msg.Data[4] = value0;
	tx_msg.Data[5] = value1;
	feidao_can_app_fill_crc(tx_msg.Data);
	(void)Can_HDX_Transmit(&tx_msg);
}

static void feidao_can_app_send_word_frame(UINT8 seq, UINT16 value)
{
	CanTxMsg tx_msg;

	memset(&tx_msg, 0, sizeof(tx_msg));
	tx_msg.StdId = FEIDAO_CAN_APP_ACK_ID;
	tx_msg.IDE = CAN_ID_STD;
	tx_msg.RTR = CAN_RTR_DATA;
	tx_msg.DLC = 8U;
	tx_msg.Data[0] = 0x5AU;
	tx_msg.Data[1] = 0xA5U;
	tx_msg.Data[2] = FEIDAO_CAN_APP_CMD_READ_BLOCK_DATA;
	tx_msg.Data[3] = seq;
	tx_msg.Data[4] = (UINT8)(value >> 8);
	tx_msg.Data[5] = (UINT8)value;
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
	s_u8AppCmdQueueHead = 0U;
	s_u8AppCmdQueueTail = 0U;
	s_u8AppCmdQueueCount = 0U;
	__enable_irq();
}

static UINT8 feidao_can_take_app_cmd(UINT8 data[8])
{
	UINT8 has_cmd = 0U;

	__disable_irq();
	if (s_u8AppCmdQueueCount != 0U)
	{
		memcpy(data, s_u8AppCmdQueue[s_u8AppCmdQueueHead], 8U);
		s_u8AppCmdQueueHead++;
		if (s_u8AppCmdQueueHead >= FEIDAO_CAN_APP_CMD_QUEUE_SIZE)
		{
			s_u8AppCmdQueueHead = 0U;
		}
		s_u8AppCmdQueueCount--;
		has_cmd = 1U;
	}
	__enable_irq();

	return has_cmd;
}

static void feidao_can_queue_app_cmd(const UINT8 data[8])
{
	if (s_u8AppCmdQueueCount >= FEIDAO_CAN_APP_CMD_QUEUE_SIZE)
	{
		feidao_can_record_tx_no_mailbox();
		return;
	}

	memcpy(s_u8AppCmdQueue[s_u8AppCmdQueueTail], data, 8U);
	s_u8AppCmdQueueTail++;
	if (s_u8AppCmdQueueTail >= FEIDAO_CAN_APP_CMD_QUEUE_SIZE)
	{
		s_u8AppCmdQueueTail = 0U;
	}
	s_u8AppCmdQueueCount++;
}

static void feidao_can_start_read_block_stream(UINT8 count)
{
	s_u8ReadBlockCount = count;
	s_u8ReadBlockIndex = 0U;
	s_u8ReadBlockActive = 1U;
	s_u32ReadBlockLastTick = s_u32CanTick - FEIDAO_CAN_APP_READ_BLOCK_FRAME_INTERVAL_TICKS;
}

static void feidao_can_stop_read_block_stream(void)
{
	s_u8ReadBlockActive = 0U;
	s_u8ReadBlockCount = 0U;
	s_u8ReadBlockIndex = 0U;
}

static void feidao_can_service_read_block_stream(UINT32 now_tick)
{
	if (s_u8ReadBlockActive == 0U)
	{
		return;
	}
	if (s_u8ReadBlockIndex >= s_u8ReadBlockCount)
	{
		feidao_can_stop_read_block_stream();
		return;
	}
	if (s_u8TxQueueCount > (FEIDAO_CAN_TX_QUEUE_SIZE - 4U))
	{
		return;
	}
	if (feidao_can_tick_elapsed(now_tick,
								s_u32ReadBlockLastTick,
								FEIDAO_CAN_APP_READ_BLOCK_FRAME_INTERVAL_TICKS) == 0U)
	{
		return;
	}

	feidao_can_app_send_word_frame(s_u8ReadBlockIndex, s_u16ReadBlockWords[s_u8ReadBlockIndex]);
	s_u8ReadBlockIndex++;
	s_u32ReadBlockLastTick = now_tick;
	if (s_u8ReadBlockIndex >= s_u8ReadBlockCount)
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
	if (cmd != FEIDAO_CAN_APP_CMD_READ_BLOCK_DATA)
	{
		feidao_can_stop_read_block_stream();
	}

	switch (cmd)
	{
	case FEIDAO_CAN_APP_CMD_GET_STATUS:
		value0 = feidao_can_u16_to_percent(g_stCellInfoReport.SocElement.u16Soc);
		value1 = feidao_can_u16_to_percent(g_stCellInfoReport.SocElement.u16Soh);
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
		s_u8EnterIapDelayTicks = FEIDAO_CAN_APP_ENTER_IAP_DELAY_TICKS;
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
		host_error = Sci_HostReadWords(reg_addr, reg_count, s_u16ReadBlockWords);
		status = feidao_can_app_status_from_host_error(host_error);
		if (status == FEIDAO_CAN_APP_ACK_OK)
		{
			value0 = reg_count;
			value1 = 0U;
			feidao_can_start_read_block_stream(reg_count);
		}
		break;

	case FEIDAO_CAN_APP_CMD_WRITE_PREP:
		s_u16WriteAddr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		s_u8WriteValueHi = data[5];
		s_u8WritePending = 1U;
		value0 = data[3];
		value1 = data[4];
		break;

	case FEIDAO_CAN_APP_CMD_WRITE_COMMIT:
		reg_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		if ((s_u8WritePending == 0U) || (reg_addr != s_u16WriteAddr))
		{
			s_u8WritePending = 0U;
			status = FEIDAO_CAN_APP_ACK_BAD_PARAM;
			break;
		}
		reg_value = (UINT16)(((UINT16)s_u8WriteValueHi << 8) | data[5]);
		s_u8WritePending = 0U;
		host_error = Sci_HostWriteWords(reg_addr, &reg_value, 1U);
		status = feidao_can_app_status_from_host_error(host_error);
		break;

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

	default:
		status = FEIDAO_CAN_APP_ACK_BAD_CMD;
		break;
	}

	feidao_can_app_send_ack(cmd, status, value0, value1);
}

static void feidao_can_service_app_cmd(void)
{
	UINT8 data[8];

	if (feidao_can_take_app_cmd(data) != 0U)
	{
		feidao_can_handle_app_cmd_data(data);
	}
}

static void feidao_can_service_enter_iap_delay(void)
{
	if ((s_u8EnterIapDelayTicks == 0U) || (0 == g_st_SysTimeFlag.bits.b1Sys10msFlag))
	{
		return;
	}

	s_u8EnterIapDelayTicks--;
	if (s_u8EnterIapDelayTicks == 0U)
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

	s_u8BusActive = 1U;
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
	can.CAN_NART = DISABLE;
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
	s_u8CanBusOff = 0U;
	s_u8TxMailbox = CAN_TxStatus_NoMailBox;
	s_u8ScheduleInit = 0U;
	s_u8BusActive = 0U;
	s_u8RtcServiceActive = 0U;
	s_u8LastRtcWakeTimeout = 0U;
	s_u8EnterIapDelayTicks = 0U;
	s_u8ReadBlockActive = 0U;
	feidao_can_clear_tx_queue();
	feidao_can_clear_app_cmd_queue();
	InitCan_GPIO();
	InitCan_NVIC();
	InitCan_CAN1();
	InitCan_Filter();
	feidao_can_update_debug_status();
}

UINT8 Can_IsBusy(void)
{
	if (s_u8TxQueueCount != 0U)
	{
		return 1U;
	}
	if (s_u8TxMailbox != CAN_TxStatus_NoMailBox)
	{
		return 1U;
	}
	if (s_u8ReadBlockActive != 0U)
	{
		return 1U;
	}
	return ((CAN1->TSR & CAN_TSR_TME) != CAN_TSR_TME) ? 1U : 0U;
}

void Can_PrepareSleep(void)
{
	feidao_can_inc_u16(&s_u16PrepareSleepCnt);
	if (s_u8TxMailbox != CAN_TxStatus_NoMailBox)
	{
		feidao_can_cancel_tx(s_u8TxMailbox);
		s_u8TxMailbox = CAN_TxStatus_NoMailBox;
	}
	feidao_can_clear_tx_queue();
	feidao_can_clear_app_cmd_queue();
	feidao_can_stop_read_block_stream();
	GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_OFF_LEVEL);
	feidao_can_update_debug_status();
}

UINT8 Can_IsBusActive(void)
{
	return s_u8BusActive;
}

UINT32 Can_GetIdleRtcPeriodSeconds(void)
{
	return FEIDAO_CAN_RTC_PERIOD_SECONDS;
}

void Can_RtcWakeService(UINT32 elapsed_seconds)
{
	UINT32 waited = 0U;
	(void)elapsed_seconds;

	GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_ON_LEVEL);
	s_u8RtcServiceActive = 1U;
	s_u8LastRtcWakeTimeout = 0U;
	feidao_can_inc_u16(&s_u16RtcWakeServiceCnt);
	feidao_can_queue_periodic_mask(CAN_FEIDAO_1000MS_MSG_MASK);

	while (Can_IsBusy() && (waited < FEIDAO_CAN_RTC_SERVICE_TIMEOUT_TICKS))
	{
		Feed_IWatchDog;
		__delay_ms(10);
		s_u32CanTick++;
		feidao_can_busoff_monitor();
		feidao_can_service_tx(s_u32CanTick);
		feidao_can_service_read_block_stream(s_u32CanTick);
		waited++;
	}
	if (Can_IsBusy())
	{
		s_u8LastRtcWakeTimeout = 1U;
	}
	s_u8RtcServiceActive = 0U;
	feidao_can_update_debug_status();
}

void App_Can(void)
{
	UINT32 now_tick = SysTime_Get10msTickCount();

	s_u32CanTick = now_tick;
	feidao_can_busoff_monitor();
	feidao_can_schedule_periodic(now_tick);
	feidao_can_service_app_cmd();
	feidao_can_service_read_block_stream(now_tick);
	feidao_can_service_tx(now_tick);
	feidao_can_service_enter_iap_delay();
	feidao_can_update_debug_status();
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
	CanRxMsg rx_msg;

	while (CAN_MessagePending(CAN1, CAN_FIFO0) != 0U)
	{
		sys_time.can_rcv_cnt++;
		CAN_Receive(CAN1, CAN_FIFO0, &rx_msg);
		feidao_can_handle_rx_msg(&rx_msg);
	}
}
