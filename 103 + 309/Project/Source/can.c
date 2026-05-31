#include "main.h"
#include "can.h"
#include "can_app.h"
#include <string.h>

#if PROJECT_CFG_DEBUG_WATCH_ENABLE
volatile struct CAN_ERROR_SNAPSHOT g_stCanErrorSnapshot;
volatile struct CAN_LOW_POWER_STATUS g_stCanLowPowerStatus;
#define FEIDAO_CAN_ERROR_INC(field) feidao_can_inc_u16(&g_stCanErrorSnapshot.field)
#else
#define FEIDAO_CAN_ERROR_INC(field) do { } while (0)
#endif

#define FEIDAO_CAN_POWER_ON_LEVEL Bit_RESET
#define FEIDAO_CAN_POWER_OFF_LEVEL Bit_SET

#define FEIDAO_CAN_PERIOD_1000MS_TICKS ((UINT32)100U)
#define FEIDAO_CAN_PERIOD_5000MS_TICKS ((UINT32)500U)
#define FEIDAO_CAN_TX_TIMEOUT_TICKS ((UINT32)20U)
#ifndef PROJECT_CFG_CAN_RTC_WAKE_PERIOD_SECONDS
#define PROJECT_CFG_CAN_RTC_WAKE_PERIOD_SECONDS 1
#endif
#ifndef PROJECT_CFG_CAN_RTC_IDLE_PERIOD_SECONDS
#define PROJECT_CFG_CAN_RTC_IDLE_PERIOD_SECONDS 10
#endif
#ifndef PROJECT_CFG_CAN_BUS_ACTIVE_HOLD_SECONDS
#define PROJECT_CFG_CAN_BUS_ACTIVE_HOLD_SECONDS 10
#endif
#ifndef PROJECT_CFG_CAN_POWER_STABLE_TICKS
#define PROJECT_CFG_CAN_POWER_STABLE_TICKS 2
#endif
#ifndef PROJECT_CFG_CAN_NO_ACK_BACKOFF_THRESHOLD
#define PROJECT_CFG_CAN_NO_ACK_BACKOFF_THRESHOLD 3
#endif
#ifndef PROJECT_CFG_CAN_PROBE_PERIOD_SECONDS
#define PROJECT_CFG_CAN_PROBE_PERIOD_SECONDS 10
#endif
#define FEIDAO_CAN_RTC_ACTIVE_PERIOD_SECONDS ((UINT32)PROJECT_CFG_CAN_RTC_WAKE_PERIOD_SECONDS)
#define FEIDAO_CAN_RTC_IDLE_PERIOD_SECONDS ((UINT32)PROJECT_CFG_CAN_RTC_IDLE_PERIOD_SECONDS)
#define FEIDAO_CAN_BUS_ACTIVE_HOLD_TICKS \
	((UINT32)PROJECT_CFG_CAN_BUS_ACTIVE_HOLD_SECONDS * FEIDAO_CAN_PERIOD_1000MS_TICKS)
#define FEIDAO_CAN_RTC_SERVICE_TIMEOUT_TICKS ((UINT32)150U)
#define FEIDAO_CAN_POWER_STABLE_TICKS ((UINT32)PROJECT_CFG_CAN_POWER_STABLE_TICKS)
#define FEIDAO_CAN_NO_ACK_BACKOFF_THRESHOLD ((UINT8)PROJECT_CFG_CAN_NO_ACK_BACKOFF_THRESHOLD)
#define FEIDAO_CAN_PROBE_PERIOD_TICKS \
	((UINT32)PROJECT_CFG_CAN_PROBE_PERIOD_SECONDS * FEIDAO_CAN_PERIOD_1000MS_TICKS)

#define FEIDAO_CAN_TX_QUEUE_SIZE ((UINT8)32U)

#define FEIDAO_CAN_TME_FLAG(mailbox) ((UINT32)(CAN_TSR_TME0 << (mailbox)))
#define FEIDAO_CAN_RQCP_FLAG(mailbox) ((UINT32)(0x38000000U | (1UL << ((mailbox) * 8U))))

typedef struct
{
	CanTxMsg frame;
} FeidaoCanTxItem;

typedef struct
{
	FeidaoCanTxItem queue[FEIDAO_CAN_TX_QUEUE_SIZE];
	UINT8 head;
	UINT8 tail;
	UINT8 count;
	UINT8 mailbox;
	UINT32 start_tick;
} FeidaoCanTxRuntime;

typedef struct
{
	UINT32 tick;
	UINT32 last_1000ms_tick;
	UINT32 last_5000ms_tick;
	UINT8 schedule_init;
	volatile UINT8 bus_active;
	volatile UINT32 last_bus_activity_tick;
	UINT8 bus_off;
	UINT8 power_on;
	UINT8 no_ack_cnt;
	UINT8 probe_active;
	UINT32 power_on_tick;
	UINT32 last_probe_tick;
	volatile UINT8 rtc_service_active;
	volatile UINT8 last_rtc_wake_timeout;
	volatile UINT8 last_rtc_wake_tx_acked;
	UINT32 last_rtc_elapsed_seconds;
	UINT16 rtc_wake_service_cnt;
	UINT16 prepare_sleep_cnt;
	UINT16 busoff_enter_cnt;
	UINT16 busoff_recover_cnt;
} FeidaoCanRuntime;

static CanAppRuntime s_app;

static FeidaoCanTxRuntime s_tx = {
	{0},
	0U,
	0U,
	0U,
	CAN_TxStatus_NoMailBox,
	0U
};
static FeidaoCanRuntime s_runtime;

static void feidao_can_inc_u16(volatile UINT16 *counter);
static UINT8 feidao_can_tick_elapsed(UINT32 now_tick, UINT32 start_tick, UINT32 wait_ticks);
static void feidao_can_update_error_snapshot(void);
static void feidao_can_record_tx_failed(void);
static void feidao_can_record_tx_timeout(void);
static void feidao_can_record_tx_no_mailbox(void);
static void feidao_can_update_debug_status(void);
static void feidao_can_power_on(UINT32 now_tick);
static void feidao_can_power_off(void);
static UINT8 feidao_can_power_ready(UINT32 now_tick);
static void feidao_can_power_down_if_idle(void);
static void feidao_can_clear_tx_done(UINT8 mailbox);
static void feidao_can_cancel_tx(UINT8 mailbox);
static UINT8 feidao_can_enqueue_tx(const CanTxMsg *frame);
static UINT8 feidao_can_dequeue_tx(CanTxMsg *frame);
static void feidao_can_clear_tx_queue(void);
static void feidao_can_record_no_ack(void);
static void feidao_can_service_tx(UINT32 now_tick);
static void feidao_can_queue_periodic_mask(UINT16 mask);
static void feidao_can_schedule_periodic(UINT32 now_tick);
static void feidao_can_busoff_monitor(void);
static void feidao_can_clear_app_cmd_queue(void);
static UINT8 feidao_can_take_app_cmd(UINT8 data[8]);
static void feidao_can_queue_app_cmd(const UINT8 data[8]);
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

static UINT32 feidao_can_period_seconds(UINT32 seconds)
{
	return (seconds == 0U) ? 1U : seconds;
}

static void feidao_can_power_on(UINT32 now_tick)
{
	if (s_runtime.power_on == 0U)
	{
		GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_ON_LEVEL);
		s_runtime.power_on = 1U;
		s_runtime.power_on_tick = now_tick;
	}
}

static void feidao_can_power_off(void)
{
	if (s_runtime.power_on != 0U)
	{
		GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_OFF_LEVEL);
		s_runtime.power_on = 0U;
	}
}

static UINT8 feidao_can_power_ready(UINT32 now_tick)
{
	if (s_runtime.power_on == 0U)
	{
		return 0U;
	}
	if (FEIDAO_CAN_POWER_STABLE_TICKS == 0U)
	{
		return 1U;
	}
	return feidao_can_tick_elapsed(now_tick,
								  s_runtime.power_on_tick,
								  FEIDAO_CAN_POWER_STABLE_TICKS);
}

static void feidao_can_power_down_if_idle(void)
{
	if ((s_runtime.bus_active == 0U) &&
		(s_runtime.rtc_service_active == 0U) &&
		(s_tx.count == 0U) &&
		(s_tx.mailbox == CAN_TxStatus_NoMailBox) &&
		(CanApp_IsReadBlockActive(&s_app) == 0U))
	{
		feidao_can_power_off();
	}
}

static void feidao_can_mark_bus_active(UINT32 now_tick)
{
	s_runtime.bus_active = 1U;
	s_runtime.last_bus_activity_tick = now_tick;
	s_runtime.no_ack_cnt = 0U;
	s_runtime.probe_active = 0U;
	if (s_runtime.rtc_service_active != 0U)
	{
		s_runtime.last_rtc_wake_tx_acked = 1U;
	}
}

static void feidao_can_update_bus_active_timeout(UINT32 now_tick)
{
	if ((s_runtime.bus_active != 0U) &&
		(feidao_can_tick_elapsed(now_tick,
								 s_runtime.last_bus_activity_tick,
								 FEIDAO_CAN_BUS_ACTIVE_HOLD_TICKS) != 0U))
	{
		s_runtime.bus_active = 0U;
		feidao_can_power_down_if_idle();
	}
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

static void feidao_can_record_tx_failed(void)
{
	feidao_can_update_error_snapshot();
	FEIDAO_CAN_ERROR_INC(u16TxFailedCnt);
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
	if (CAN_ErrorCode_ACKErr == g_stCanErrorSnapshot.u8LastErrorCode)
	{
		FEIDAO_CAN_ERROR_INC(u16AckErrorCnt);
	}
#endif
}

static void feidao_can_record_tx_timeout(void)
{
	feidao_can_update_error_snapshot();
	FEIDAO_CAN_ERROR_INC(u16TxTimeoutCnt);
}

static void feidao_can_record_no_ack(void)
{
	if (s_runtime.no_ack_cnt < (UINT8)0xFFU)
	{
		s_runtime.no_ack_cnt++;
	}
	if (s_runtime.no_ack_cnt >= FEIDAO_CAN_NO_ACK_BACKOFF_THRESHOLD)
	{
		s_runtime.bus_active = 0U;
		s_runtime.probe_active = 0U;
		feidao_can_clear_tx_queue();
		s_app.read_block_active = 0U;
	}
}

static void feidao_can_record_tx_no_mailbox(void)
{
	feidao_can_update_error_snapshot();
	FEIDAO_CAN_ERROR_INC(u16TxNoMailboxCnt);
}

static void feidao_can_update_debug_status(void)
{
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
	g_stCanLowPowerStatus.u8PowerState = s_runtime.power_on;
	g_stCanLowPowerStatus.u8BusActive = s_runtime.bus_active;
	g_stCanLowPowerStatus.u8NoAckCnt = s_runtime.no_ack_cnt;
	g_stCanLowPowerStatus.u8ProbeActive = s_runtime.probe_active;
	g_stCanLowPowerStatus.u8TxMailbox = s_tx.mailbox;
	g_stCanLowPowerStatus.u8RtcServiceActive = s_runtime.rtc_service_active;
	g_stCanLowPowerStatus.u8LastRtcWakeTxAcked = s_runtime.last_rtc_wake_tx_acked;
	g_stCanLowPowerStatus.u8LastRtcWakeTimeout = s_runtime.last_rtc_wake_timeout;
	g_stCanLowPowerStatus.u16PendingMask = s_tx.count;
	g_stCanLowPowerStatus.u16RtcWakeServiceCnt = s_runtime.rtc_wake_service_cnt;
	g_stCanLowPowerStatus.u16PrepareSleepCnt = s_runtime.prepare_sleep_cnt;
	g_stCanLowPowerStatus.u32LogicalTick = s_runtime.tick;
	g_stCanLowPowerStatus.u32LastRtcElapsedSeconds = s_runtime.last_rtc_elapsed_seconds;
#else
	(void)s_runtime.rtc_service_active;
	(void)s_runtime.last_rtc_wake_timeout;
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
	FEIDAO_CAN_ERROR_INC(u16TxAbortCnt);
}

static UINT8 feidao_can_enqueue_tx(const CanTxMsg *frame)
{
	if ((frame == 0) || (frame->DLC > 8U) || (s_tx.count >= FEIDAO_CAN_TX_QUEUE_SIZE))
	{
		feidao_can_record_tx_no_mailbox();
		return 0U;
	}

	s_tx.queue[s_tx.tail].frame = *frame;
	s_tx.tail++;
	if (s_tx.tail >= FEIDAO_CAN_TX_QUEUE_SIZE)
	{
		s_tx.tail = 0U;
	}
	s_tx.count++;
	return 1U;
}

static UINT8 feidao_can_dequeue_tx(CanTxMsg *frame)
{
	if ((frame == 0) || (s_tx.count == 0U))
	{
		return 0U;
	}

	*frame = s_tx.queue[s_tx.head].frame;
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

static void feidao_can_service_tx(UINT32 now_tick)
{
	CanTxMsg frame;
	UINT8 status;

	if (s_tx.mailbox != CAN_TxStatus_NoMailBox)
	{
		status = CAN_TransmitStatus(CAN1, s_tx.mailbox);
		if (status == CAN_TxStatus_Ok)
		{
			feidao_can_mark_bus_active(now_tick);
			feidao_can_clear_tx_done(s_tx.mailbox);
			s_tx.mailbox = CAN_TxStatus_NoMailBox;
		}
		else if (status == CAN_TxStatus_Failed)
		{
			feidao_can_record_tx_failed();
			feidao_can_record_no_ack();
			feidao_can_clear_tx_done(s_tx.mailbox);
			s_tx.mailbox = CAN_TxStatus_NoMailBox;
		}
		else if (feidao_can_tick_elapsed(now_tick, s_tx.start_tick, FEIDAO_CAN_TX_TIMEOUT_TICKS))
		{
			feidao_can_record_tx_timeout();
			feidao_can_record_no_ack();
			feidao_can_cancel_tx(s_tx.mailbox);
			s_tx.mailbox = CAN_TxStatus_NoMailBox;
		}
	}

	if ((s_tx.mailbox == CAN_TxStatus_NoMailBox) &&
		(s_runtime.bus_off == 0U) &&
		(s_tx.count != 0U))
	{
		if (feidao_can_power_ready(now_tick) == 0U)
		{
			feidao_can_power_on(now_tick);
			return;
		}
		if (feidao_can_dequeue_tx(&frame) == 0U)
		{
			feidao_can_power_down_if_idle();
			return;
		}
		s_tx.mailbox = CAN_Transmit(CAN1, &frame);
		if (s_tx.mailbox == CAN_TxStatus_NoMailBox)
		{
			feidao_can_record_tx_no_mailbox();
		}
		else
		{
			s_tx.start_tick = now_tick;
		}
	}
	feidao_can_power_down_if_idle();
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
	if (s_runtime.schedule_init == 0U)
	{
		s_runtime.schedule_init = 1U;
		s_runtime.last_1000ms_tick = now_tick;
		s_runtime.last_5000ms_tick = now_tick;
		s_runtime.last_probe_tick = now_tick - FEIDAO_CAN_PROBE_PERIOD_TICKS;
	}

	if (s_runtime.bus_active == 0U)
	{
		if (feidao_can_tick_elapsed(now_tick,
								 s_runtime.last_probe_tick,
								 FEIDAO_CAN_PROBE_PERIOD_TICKS) != 0U)
		{
			s_runtime.last_probe_tick = now_tick;
			s_runtime.probe_active = 1U;
			feidao_can_queue_periodic_mask(CAN_FEIDAO_RTC_PROBE_MSG_MASK);
		}
		return;
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

static void feidao_can_busoff_monitor(void)
{
	UINT8 bus_off = ((CAN1->ESR & CAN_ESR_BOFF) != 0U) ? 1U : 0U;

	if ((bus_off != 0U) && (s_runtime.bus_off == 0U))
	{
		s_runtime.bus_off = 1U;
		s_runtime.busoff_enter_cnt++;
		feidao_can_update_error_snapshot();
		FEIDAO_CAN_ERROR_INC(u16BusOffCnt);
		if (s_tx.mailbox != CAN_TxStatus_NoMailBox)
		{
			feidao_can_cancel_tx(s_tx.mailbox);
			s_tx.mailbox = CAN_TxStatus_NoMailBox;
		}
		s_runtime.bus_active = 0U;
		s_runtime.probe_active = 0U;
		feidao_can_clear_tx_queue();
		feidao_can_power_down_if_idle();
	}
	else if ((bus_off == 0U) && (s_runtime.bus_off != 0U))
	{
		s_runtime.bus_off = 0U;
		s_runtime.busoff_recover_cnt++;
	}
}

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

static UINT32 feidao_can_period_seconds(UINT32 seconds)
{
	return (seconds == 0U) ? 1U : seconds;
}

static void feidao_can_power_on(UINT32 now_tick)
{
	if (s_runtime.power_on == 0U)
	{
		GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_ON_LEVEL);
		s_runtime.power_on = 1U;
		s_runtime.power_on_tick = now_tick;
	}
}

static void feidao_can_power_off(void)
{
	if (s_runtime.power_on != 0U)
	{
		GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_OFF_LEVEL);
		s_runtime.power_on = 0U;
	}
}

static UINT8 feidao_can_power_ready(UINT32 now_tick)
{
	if (s_runtime.power_on == 0U)
	{
		return 0U;
	}
	if (FEIDAO_CAN_POWER_STABLE_TICKS == 0U)
	{
		return 1U;
	}
	return feidao_can_tick_elapsed(now_tick,
								  s_runtime.power_on_tick,
								  FEIDAO_CAN_POWER_STABLE_TICKS);
}

static void feidao_can_power_down_if_idle(void)
{
	if ((s_runtime.bus_active == 0U) &&
		(s_runtime.rtc_service_active == 0U) &&
		(s_tx.count == 0U) &&
		(s_tx.mailbox == CAN_TxStatus_NoMailBox) &&
		(CanApp_IsReadBlockActive(&s_app) == 0U))
	{
		feidao_can_power_off();
	}
}

static void feidao_can_mark_bus_active(UINT32 now_tick)
{
	s_runtime.bus_active = 1U;
	s_runtime.last_bus_activity_tick = now_tick;
	s_runtime.no_ack_cnt = 0U;
	s_runtime.probe_active = 0U;
	if (s_runtime.rtc_service_active != 0U)
	{
		s_runtime.last_rtc_wake_tx_acked = 1U;
	}
}

static void feidao_can_update_bus_active_timeout(UINT32 now_tick)
{
	if ((s_runtime.bus_active != 0U) &&
		(feidao_can_tick_elapsed(now_tick,
								 s_runtime.last_bus_activity_tick,
								 FEIDAO_CAN_BUS_ACTIVE_HOLD_TICKS) != 0U))
	{
		s_runtime.bus_active = 0U;
		feidao_can_power_down_if_idle();
	}
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

static void feidao_can_record_tx_failed(void)
{
	feidao_can_update_error_snapshot();
	FEIDAO_CAN_ERROR_INC(u16TxFailedCnt);
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
	if (CAN_ErrorCode_ACKErr == g_stCanErrorSnapshot.u8LastErrorCode)
	{
		FEIDAO_CAN_ERROR_INC(u16AckErrorCnt);
	}
#endif
}

static void feidao_can_record_tx_timeout(void)
{
	feidao_can_update_error_snapshot();
	FEIDAO_CAN_ERROR_INC(u16TxTimeoutCnt);
}

static void feidao_can_record_no_ack(void)
{
	if (s_runtime.no_ack_cnt < (UINT8)0xFFU)
	{
		s_runtime.no_ack_cnt++;
	}
	if (s_runtime.no_ack_cnt >= FEIDAO_CAN_NO_ACK_BACKOFF_THRESHOLD)
	{
		s_runtime.bus_active = 0U;
		s_runtime.probe_active = 0U;
		feidao_can_clear_tx_queue();
		s_app.read_block_active = 0U;
	}
}

static void feidao_can_record_tx_no_mailbox(void)
{
	feidao_can_update_error_snapshot();
	FEIDAO_CAN_ERROR_INC(u16TxNoMailboxCnt);
}

static void feidao_can_update_debug_status(void)
{
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
	g_stCanLowPowerStatus.u8PowerState = s_runtime.power_on;
	g_stCanLowPowerStatus.u8BusActive = s_runtime.bus_active;
	g_stCanLowPowerStatus.u8NoAckCnt = s_runtime.no_ack_cnt;
	g_stCanLowPowerStatus.u8ProbeActive = s_runtime.probe_active;
	g_stCanLowPowerStatus.u8TxMailbox = s_tx.mailbox;
	g_stCanLowPowerStatus.u8RtcServiceActive = s_runtime.rtc_service_active;
	g_stCanLowPowerStatus.u8LastRtcWakeTxAcked = s_runtime.last_rtc_wake_tx_acked;
	g_stCanLowPowerStatus.u8LastRtcWakeTimeout = s_runtime.last_rtc_wake_timeout;
	g_stCanLowPowerStatus.u16PendingMask = s_tx.count;
	g_stCanLowPowerStatus.u16RtcWakeServiceCnt = s_runtime.rtc_wake_service_cnt;
	g_stCanLowPowerStatus.u16PrepareSleepCnt = s_runtime.prepare_sleep_cnt;
	g_stCanLowPowerStatus.u32LogicalTick = s_runtime.tick;
	g_stCanLowPowerStatus.u32LastRtcElapsedSeconds = s_runtime.last_rtc_elapsed_seconds;
#else
	(void)s_runtime.rtc_service_active;
	(void)s_runtime.last_rtc_wake_timeout;
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
	FEIDAO_CAN_ERROR_INC(u16TxAbortCnt);
}

static UINT8 feidao_can_enqueue_tx(const CanTxMsg *frame)
{
	if ((frame == 0) || (frame->DLC > 8U) || (s_tx.count >= FEIDAO_CAN_TX_QUEUE_SIZE))
	{
		feidao_can_record_tx_no_mailbox();
		return 0U;
	}

	s_tx.queue[s_tx.tail].frame = *frame;
	s_tx.tail++;
	if (s_tx.tail >= FEIDAO_CAN_TX_QUEUE_SIZE)
	{
		s_tx.tail = 0U;
	}
	s_tx.count++;
	return 1U;
}

static UINT8 feidao_can_dequeue_tx(CanTxMsg *frame)
{
	if ((frame == 0) || (s_tx.count == 0U))
	{
		return 0U;
	}

	*frame = s_tx.queue[s_tx.head].frame;
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

static void feidao_can_service_tx(UINT32 now_tick)
{
	CanTxMsg frame;
	UINT8 status;

	if (s_tx.mailbox != CAN_TxStatus_NoMailBox)
	{
		status = CAN_TransmitStatus(CAN1, s_tx.mailbox);
		if (status == CAN_TxStatus_Ok)
		{
			feidao_can_mark_bus_active(now_tick);
			feidao_can_clear_tx_done(s_tx.mailbox);
			s_tx.mailbox = CAN_TxStatus_NoMailBox;
		}
		else if (status == CAN_TxStatus_Failed)
		{
			feidao_can_record_tx_failed();
			feidao_can_record_no_ack();
			feidao_can_clear_tx_done(s_tx.mailbox);
			s_tx.mailbox = CAN_TxStatus_NoMailBox;
		}
		else if (feidao_can_tick_elapsed(now_tick, s_tx.start_tick, FEIDAO_CAN_TX_TIMEOUT_TICKS))
		{
			feidao_can_record_tx_timeout();
			feidao_can_record_no_ack();
			feidao_can_cancel_tx(s_tx.mailbox);
			s_tx.mailbox = CAN_TxStatus_NoMailBox;
		}
	}

	if ((s_tx.mailbox == CAN_TxStatus_NoMailBox) &&
		(s_runtime.bus_off == 0U) &&
		(s_tx.count != 0U))
	{
		if (feidao_can_power_ready(now_tick) == 0U)
		{
			feidao_can_power_on(now_tick);
			return;
		}
		if (feidao_can_dequeue_tx(&frame) == 0U)
		{
			feidao_can_power_down_if_idle();
			return;
		}
		s_tx.mailbox = CAN_Transmit(CAN1, &frame);
		if (s_tx.mailbox == CAN_TxStatus_NoMailBox)
		{
			feidao_can_record_tx_no_mailbox();
		}
		else
		{
			s_tx.start_tick = now_tick;
		}
	}
	feidao_can_power_down_if_idle();
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
	if (s_runtime.schedule_init == 0U)
	{
		s_runtime.schedule_init = 1U;
		s_runtime.last_1000ms_tick = now_tick;
		s_runtime.last_5000ms_tick = now_tick;
		s_runtime.last_probe_tick = now_tick - FEIDAO_CAN_PROBE_PERIOD_TICKS;
	}

	if (s_runtime.bus_active == 0U)
	{
		if (feidao_can_tick_elapsed(now_tick,
								 s_runtime.last_probe_tick,
								 FEIDAO_CAN_PROBE_PERIOD_TICKS) != 0U)
		{
			s_runtime.last_probe_tick = now_tick;
			s_runtime.probe_active = 1U;
			feidao_can_queue_periodic_mask(CAN_FEIDAO_RTC_PROBE_MSG_MASK);
		}
		return;
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

static void feidao_can_busoff_monitor(void)
{
	UINT8 bus_off = ((CAN1->ESR & CAN_ESR_BOFF) != 0U) ? 1U : 0U;

	if ((bus_off != 0U) && (s_runtime.bus_off == 0U))
	{
		s_runtime.bus_off = 1U;
		s_runtime.busoff_enter_cnt++;
		feidao_can_update_error_snapshot();
		FEIDAO_CAN_ERROR_INC(u16BusOffCnt);
		if (s_tx.mailbox != CAN_TxStatus_NoMailBox)
		{
			feidao_can_cancel_tx(s_tx.mailbox);
			s_tx.mailbox = CAN_TxStatus_NoMailBox;
		}
		s_runtime.bus_active = 0U;
		s_runtime.probe_active = 0U;
		feidao_can_clear_tx_queue();
		feidao_can_power_down_if_idle();
	}
	else if ((bus_off == 0U) && (s_runtime.bus_off != 0U))
	{
		s_runtime.bus_off = 0U;
		s_runtime.busoff_recover_cnt++;
	}
}

	{
		u8FlashUpdateFlag = 1U;
	}
}

static void feidao_can_handle_rx_msg(const CanRxMsg *rx_msg)
{
	UINT16 expect_std_id = (UINT16)(((UINT16)CAN_ADRESS_STD_ID << 7) | CAN_APP_CMD_ID);

	if ((rx_msg == 0) || (rx_msg->IDE != CAN_ID_STD))
	{
		return;
	}

	feidao_can_mark_bus_active(SysTime_Get10msTickCount());
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

	GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_OFF_LEVEL);
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
	UINT32 now_tick = SysTime_Get10msTickCount();
	UINT8 keep_bus_active = s_runtime.bus_active;
	UINT32 keep_bus_activity_tick = s_runtime.last_bus_activity_tick;

	s_runtime.bus_off = 0U;
	s_tx.mailbox = CAN_TxStatus_NoMailBox;
	s_runtime.schedule_init = 0U;
	s_runtime.bus_active = keep_bus_active;
	s_runtime.last_bus_activity_tick = keep_bus_activity_tick;
	s_runtime.power_on = 0U;
	s_runtime.power_on_tick = now_tick;
	s_runtime.probe_active = 0U;
	s_runtime.rtc_service_active = 0U;
	s_runtime.last_rtc_wake_timeout = 0U;
	s_runtime.last_rtc_wake_tx_acked = 0U;
	s_app.enter_iap_delay_ticks = 0U;
	s_app.read_block_active = 0U;
	feidao_can_clear_tx_queue();
	feidao_can_clear_app_cmd_queue();
	InitCan_GPIO();
	InitCan_NVIC();
	InitCan_CAN1();
	InitCan_Filter();
	if (s_runtime.bus_active != 0U)
	{
		feidao_can_power_on(now_tick);
	}
	else
	{
		s_runtime.last_probe_tick = now_tick - FEIDAO_CAN_PROBE_PERIOD_TICKS;
	}
	feidao_can_update_debug_status();
}

UINT8 Can_IsBusy(void)
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

void Can_PrepareSleep(void)
{
	feidao_can_inc_u16(&s_runtime.prepare_sleep_cnt);
	if (s_tx.mailbox != CAN_TxStatus_NoMailBox)
	{
		feidao_can_cancel_tx(s_tx.mailbox);
		s_tx.mailbox = CAN_TxStatus_NoMailBox;
	}
	feidao_can_clear_tx_queue();
	feidao_can_clear_app_cmd_queue();
	s_app.read_block_active = 0U;
	feidao_can_power_off();
	feidao_can_update_debug_status();
}

UINT8 Can_IsBusActive(void)
{
	feidao_can_update_bus_active_timeout(SysTime_Get10msTickCount());
	return s_runtime.bus_active;
}

UINT32 Can_GetIdleRtcPeriodSeconds(void)
{
	feidao_can_update_bus_active_timeout(SysTime_Get10msTickCount());
	if (s_runtime.bus_active != 0U)
	{
		return feidao_can_period_seconds(FEIDAO_CAN_RTC_ACTIVE_PERIOD_SECONDS);
	}
	return feidao_can_period_seconds(FEIDAO_CAN_RTC_IDLE_PERIOD_SECONDS);
}

void Can_RtcWakeService(UINT32 elapsed_seconds)
{
	UINT32 waited = 0U;
	UINT8 bus_active;

	s_runtime.tick = SysTime_Get10msTickCount();
	s_runtime.last_rtc_elapsed_seconds = elapsed_seconds;
	if ((s_runtime.bus_active != 0U) &&
		(elapsed_seconds >= PROJECT_CFG_CAN_BUS_ACTIVE_HOLD_SECONDS))
	{
		s_runtime.bus_active = 0U;
	}
	else if (s_runtime.bus_active != 0U)
	{
		s_runtime.last_bus_activity_tick = s_runtime.tick;
	}
	bus_active = s_runtime.bus_active;
	feidao_can_power_on(s_runtime.tick);
	s_runtime.rtc_service_active = 1U;
	s_runtime.last_rtc_wake_timeout = 0U;
	s_runtime.last_rtc_wake_tx_acked = 0U;
	feidao_can_inc_u16(&s_runtime.rtc_wake_service_cnt);
	if (bus_active != 0U)
	{
		feidao_can_queue_periodic_mask(CAN_FEIDAO_1000MS_MSG_MASK);
		if ((elapsed_seconds >= 5U) ||
			((s_runtime.rtc_wake_service_cnt % 5U) == 0U))
		{
			feidao_can_queue_periodic_mask(CAN_FEIDAO_5000MS_MSG_MASK);
		}
	}
	else
	{
		s_runtime.probe_active = 1U;
		feidao_can_queue_periodic_mask(CAN_FEIDAO_RTC_PROBE_MSG_MASK);
	}

	while (Can_IsBusy() && (waited < FEIDAO_CAN_RTC_SERVICE_TIMEOUT_TICKS))
	{
		Feed_IWatchDog;
		__delay_ms(10);
		s_runtime.tick++;
		feidao_can_busoff_monitor();
		feidao_can_service_tx(s_runtime.tick);
		CanApp_ServiceReadBlock(s_runtime.tick, &s_app, s_runtime.tick, s_tx.count);
		waited++;
	}
	if (Can_IsBusy())
	{
		s_runtime.last_rtc_wake_timeout = 1U;
	}
	s_runtime.rtc_service_active = 0U;
	feidao_can_power_down_if_idle();
	feidao_can_update_debug_status();
}

void App_Can(void)
{
	UINT32 now_tick = SysTime_Get10msTickCount();
	UINT8 app_cmd_data[8];

	s_runtime.tick = now_tick;
	feidao_can_update_bus_active_timeout(now_tick);
	feidao_can_busoff_monitor();
	feidao_can_schedule_periodic(now_tick);
	if (feidao_can_take_app_cmd(app_cmd_data) != 0U)
	{
		CanApp_HandleCmd(app_cmd_data, &s_app);
	}
	CanApp_ServiceReadBlock(now_tick, &s_app, s_runtime.tick, s_tx.count);
	feidao_can_service_tx(now_tick);
	CanApp_ServiceEnterIapDelay(&s_app);
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

/* ── CanFeidaoFrames.c section ── */
#include "FactoryAging.h"

#define CAN_FEIDAO_EXT_ID_BASE ((UINT32)0x14F80200U)
#define CAN_FEIDAO_FRAME_LEN_8 ((uint8_t)8U)

static void CanFeidao_PutU16Be(uint8_t *data, uint8_t offset, uint16_t value);
static void CanFeidao_PutU32Be(uint8_t *data, uint8_t offset, uint32_t value);
static UINT8 CanFeidao_SendFrame(uint8_t chd_index, const uint8_t *data, uint8_t length);
static UINT8 CanFeidao_SendVoltageCurrent1000ms(void);
static UINT8 CanFeidao_SendCap5000ms(void);
static UINT8 CanFeidao_SendSoc1000ms(void);
static UINT8 CanFeidao_SendSoh5000ms(void);
static UINT8 CanFeidao_SendVersion5000ms(void);
static UINT8 CanFeidao_SendStatus5000ms(void);
static UINT8 CanFeidao_SendFactoryTime5000ms(void);

typedef UINT8 (*CanFeidao_SendHandler)(void);

typedef struct
{
	UINT16 mask;
	CanFeidao_SendHandler handler;
} CanFeidao_FrameDispatch;

static const CanFeidao_FrameDispatch s_can_feidao_dispatch[] = {
	{CAN_FEIDAO_MSG_VOLTAGE_CURRENT_1000MS, CanFeidao_SendVoltageCurrent1000ms},
	{CAN_FEIDAO_MSG_SOC_1000MS, CanFeidao_SendSoc1000ms},
	{CAN_FEIDAO_MSG_CAP_5000MS, CanFeidao_SendCap5000ms},
	{CAN_FEIDAO_MSG_SOH_5000MS, CanFeidao_SendSoh5000ms},
	{CAN_FEIDAO_MSG_VERSION_5000MS, CanFeidao_SendVersion5000ms},
	{CAN_FEIDAO_MSG_STATUS_5000MS, CanFeidao_SendStatus5000ms},
	{CAN_FEIDAO_MSG_FACTORY_TIME_5000MS, CanFeidao_SendFactoryTime5000ms},
};

static void CanFeidao_PutU16Be(uint8_t *data, uint8_t offset, uint16_t value)
{
	data[offset] = (uint8_t)((value >> 8) & 0xFFU);
	data[offset + 1U] = (uint8_t)(value & 0xFFU);
}

static void CanFeidao_PutU32Be(uint8_t *data, uint8_t offset, uint32_t value)
{
	data[offset] = (uint8_t)((value >> 24) & 0xFFU);
	data[offset + 1U] = (uint8_t)((value >> 16) & 0xFFU);
	data[offset + 2U] = (uint8_t)((value >> 8) & 0xFFU);
	data[offset + 3U] = (uint8_t)(value & 0xFFU);
}

static UINT8 CanFeidao_SendFrame(uint8_t chd_index, const uint8_t *data, uint8_t length)
{
	CanTxMsg tx_msg;
	uint8_t i;

	tx_msg.StdId = 0U;
	tx_msg.RTR = CAN_RTR_DATA;
	tx_msg.IDE = CAN_ID_EXT;
	tx_msg.ExtId = CAN_FEIDAO_EXT_ID_BASE | chd_index;
	tx_msg.DLC = length;

	for (i = 0U; i < length; i++)
	{
		tx_msg.Data[i] = data[i];
	}

	return Can_HDX_Transmit(&tx_msg);
}

static UINT8 CanFeidao_SendVoltageCurrent1000ms(void)
{
	uint8_t data[8] = {0U};
	int32_t current;
	uint32_t voltage = (uint32_t)g_stCellInfoReport.u16VCellTotle * 10U;

	if (g_stCellInfoReport.u16IDischg > 0U)
	{
		current = -(uint32_t)g_stCellInfoReport.u16IDischg * 100;
	}
	else
	{
		current = (uint32_t)g_stCellInfoReport.u16Ichg * 100U;
	}

	CanFeidao_PutU32Be(data, 0U, voltage);
	CanFeidao_PutU32Be(data, 4U, (uint32_t)current);
	return CanFeidao_SendFrame(0U, data, CAN_FEIDAO_FRAME_LEN_8);
}

static UINT8 CanFeidao_SendCap5000ms(void)
{
	uint8_t data[8] = {0U};
	uint32_t real_cap = g_stCellInfoReport.SocElement.u16CapacityNow * 10U * g_stCellInfoReport.u16VCellTotle / 100U;
	uint32_t design_cap = g_stCellInfoReport.SocElement.u16CapacityFactory * 10U * (36U * SNum) / 10U;

	if (real_cap >= design_cap)
	{
		real_cap = design_cap;
	}

	CanFeidao_PutU32Be(data, 0U, real_cap);
	CanFeidao_PutU32Be(data, 4U, design_cap);
	return CanFeidao_SendFrame(1U, data, CAN_FEIDAO_FRAME_LEN_8);
}

static UINT8 CanFeidao_SendSoc1000ms(void)
{
	uint8_t data[8] = {0U};
	uint8_t chg_status;
	uint8_t soc;
	int8_t temp;
	uint8_t bat_type;
	uint16_t time_chg = 100U;
	uint16_t res = 0U;

	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp || g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp)
	{
		chg_status = 2U;
	}
	else if (g_stCellInfoReport.u16Ichg)
	{
		chg_status = 1U;
	}
	else
	{
		chg_status = 0U;
	}

	soc = g_stCellInfoReport.SocElement.u16Soc;
	temp = (int8_t)((int16_t)g_stCellInfoReport.u16TempMax / 10 - 40);

	data[0] = chg_status;
	data[1] = soc;
	data[2] = (uint8_t)temp;
	CanFeidao_PutU16Be(data, 3U, time_chg);

#if (BAT_TYPE == BAT_MASTER)
	bat_type = 0x00U;
#elif (BAT_TYPE == BAT_SLAVE)
	bat_type = 0x01U;
#endif
	data[5] = bat_type;
	CanFeidao_PutU16Be(data, 6U, res);
	return CanFeidao_SendFrame(2U, data, CAN_FEIDAO_FRAME_LEN_8);
}

static UINT8 CanFeidao_SendSoh5000ms(void)
{
	uint8_t data[8] = {0U};
	uint8_t soh = g_stCellInfoReport.SocElement.u16Soh;
	uint16_t cycles = g_stCellInfoReport.SocElement.u16Cycle_times;

	data[0] = soh;
	CanFeidao_PutU16Be(data, 1U, cycles);
	return CanFeidao_SendFrame(3U, data, CAN_FEIDAO_FRAME_LEN_8);
}

static UINT8 CanFeidao_SendVersion5000ms(void)
{
	uint8_t data[8] = {0U};
	uint8_t pro_version = 1U;
	uint16_t soft_version = 1U;

	data[0] = pro_version;
	data[1] = soft_version;
	return CanFeidao_SendFrame(4U, data, CAN_FEIDAO_FRAME_LEN_8);
}

static UINT8 CanFeidao_SendStatus5000ms(void)
{
	uint8_t data[8] = {0U};
	uint8_t work_status = 0U;
	uint8_t exception_status = 0U;
	uint16_t cap_fac;
	uint16_t cap_now;
	uint16_t cap_design;

	work_status |= (UINT8)(SystemRuntime_IsDischargeMosOpen() << 0);
	work_status |= (UINT8)(SystemRuntime_IsChargeMosOpen() << 1);

	if (g_stCellInfoReport.u16Ichg)
	{
		work_status |= (UINT8)(1U << 2);
		work_status |= (UINT8)(1U << 3);
	}
	if (g_stCellInfoReport.u16IDischg)
	{
		work_status |= (UINT8)(1U << 4);
	}

	if (g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp)
	{
		exception_status = 0x02U;
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp)
	{
		exception_status = 0x03U;
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp)
	{
		exception_status = 0x04U;
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp)
	{
		exception_status = 0x05U;
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp || g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp)
	{
		exception_status = 0x06U;
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp || g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp)
	{
		exception_status = 0x07U;
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp)
	{
		exception_status = 0x08U;
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp)
	{
		exception_status = 0x09U;
	}
	if (System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG))
	{
		exception_status = 0x0CU;
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1VcellDeltaBig)
	{
		exception_status = 0x0DU;
	}

	cap_fac = g_stCellInfoReport.SocElement.u16CapacityFactory * 10U;
	cap_now = g_stCellInfoReport.SocElement.u16CapacityNow * 10U;
	cap_design = g_stCellInfoReport.SocElement.u16CapacityFactory * 10U;

	data[0] = work_status;
	data[1] = exception_status;
	CanFeidao_PutU16Be(data, 2U, cap_fac);
	CanFeidao_PutU16Be(data, 4U, cap_now);
	CanFeidao_PutU16Be(data, 6U, cap_design);
	return CanFeidao_SendFrame(5U, data, CAN_FEIDAO_FRAME_LEN_8);
}

static UINT8 CanFeidao_SendFactoryTime5000ms(void)
{
	uint8_t data[8] = {0U};
	UINT32 aging_remaining_min;

	CanFeidao_PutU16Be(data, 0U, g_stCellInfoReport.SocElement.u16CapacityFactory * 10U);
	data[2] = FactoryAging_GetState();
	aging_remaining_min = (FactoryAging_GetRemainingSeconds() + 59U) / 60U;
	if (aging_remaining_min > 0xFFFFU)
	{
		aging_remaining_min = 0xFFFFU;
	}
	CanFeidao_PutU16Be(data, 3U, (UINT16)aging_remaining_min);
	data[5] = FD_YEAR;
	data[6] = FD_MONTH;
	data[7] = FD_DAY;

	return CanFeidao_SendFrame(8U, data, CAN_FEIDAO_FRAME_LEN_8);
}

UINT8 CanFeidao_SendNextPending(UINT16 *pending_mask)
{
	UINT8 i;

	for (i = 0U; i < (UINT8)(sizeof(s_can_feidao_dispatch) / sizeof(s_can_feidao_dispatch[0])); ++i)
	{
		if (*pending_mask & s_can_feidao_dispatch[i].mask)
		{
			*pending_mask &= (UINT16)(~s_can_feidao_dispatch[i].mask);
			return s_can_feidao_dispatch[i].handler();
		}
	}

	return CAN_TxStatus_NoMailBox;
}
