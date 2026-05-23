#include "main.h"
#include "CanFeidaoFrames.h"

#if PROJECT_CFG_DEBUG_WATCH_ENABLE
volatile struct CAN_ERROR_SNAPSHOT g_stCanErrorSnapshot;
volatile struct CAN_LOW_POWER_STATUS g_stCanLowPowerStatus;
#define FEIDAO_CAN_ERROR_INC(field) feidao_can_inc_u16(&g_stCanErrorSnapshot.field)
#else
#define FEIDAO_CAN_ERROR_INC(field) do { } while (0)
#endif

UINT16 g_u16BusOff_InitTestCnt = 0; // CAN总线关闭计时
UINT16 g_u16BusOff_RecoverCnt = 0;	// 5s计时标志位
static UINT8 s_u8CanBusOff = 0U;
static volatile UINT8 s_u8FeidaoCanAppCmdPending = 0U;
static UINT8 s_u8FeidaoCanAppCmdData[8];
static UINT8 s_u8FeidaoCanWritePending = 0U;
static UINT16 s_u16FeidaoCanWriteAddr = 0U;
static UINT8 s_u8FeidaoCanWriteValueHi = 0U;

#define FEIDAO_CAN_POWER_ON_LEVEL Bit_RESET
#define FEIDAO_CAN_POWER_OFF_LEVEL Bit_SET

#define FEIDAO_CAN_POWER_STABLE_TICKS ((UINT32)10U)     /* 100ms for transceiver wake */
#define FEIDAO_CAN_TX_DONE_TIMEOUT_TICKS ((UINT32)20U)  /* 200ms TX wait timeout */
#define FEIDAO_CAN_PERIOD_1000MS_TICKS ((UINT32)100U)
#define FEIDAO_CAN_PERIOD_5000MS_TICKS ((UINT32)500U)
#define FEIDAO_CAN_TICKS_PER_SECOND ((UINT32)100U)
#define FEIDAO_CAN_RTC_ACTIVE_PERIOD_SECONDS ((UINT32)1U)
#define FEIDAO_CAN_RTC_IDLE_PERIOD_SECONDS ((UINT32)10U)
#define FEIDAO_CAN_RTC_SERVICE_TIMEOUT_TICKS ((UINT32)150U)
#define FEIDAO_CAN_NO_ACK_INACTIVE_LIMIT ((UINT8)6U)
#define FEIDAO_CAN_APP_RX_WINDOW_TICKS ((UINT32)20U)

#define FEIDAO_CAN_APP_CMD_ID ((UINT16)0x60U)
#define FEIDAO_CAN_APP_ACK_ID ((UINT16)0x61U)
#define FEIDAO_CAN_APP_CMD_GET_STATUS ((UINT8)0x01U)
#define FEIDAO_CAN_APP_CMD_ENTER_IAP ((UINT8)0x02U)
#define FEIDAO_CAN_APP_CMD_READ_REG ((UINT8)0x03U)
#define FEIDAO_CAN_APP_CMD_WRITE_PREP ((UINT8)0x04U)
#define FEIDAO_CAN_APP_CMD_WRITE_COMMIT ((UINT8)0x05U)
#define FEIDAO_CAN_APP_CMD_READ_BLOCK ((UINT8)0x06U)
#define FEIDAO_CAN_APP_CMD_READ_BLOCK_DATA ((UINT8)0x86U)
#define FEIDAO_CAN_APP_READ_BLOCK_MAX_WORDS ((UINT8)120U)
#define FEIDAO_CAN_APP_ACK_OK ((UINT8)0x00U)
#define FEIDAO_CAN_APP_ACK_BAD_CMD ((UINT8)0x01U)
#define FEIDAO_CAN_APP_ACK_BAD_PARAM ((UINT8)0x02U)
#define FEIDAO_CAN_APP_ACK_FLASH_ERR ((UINT8)0x05U)
#define FEIDAO_CAN_APP_ACK_NO_PERMISSION ((UINT8)0x07U)
#define FEIDAO_CAN_APP_ACK_BMS_ERROR ((UINT8)0x08U)
#define FEIDAO_CAN_APP_ENTER_IAP_DELAY_TICKS ((UINT8)20U)

#define FEIDAO_CAN_TXMAILBOX_COUNT ((UINT8)3U)
#define FEIDAO_CAN_ABORT_WAIT_LOOP ((UINT16)1000U)
#define FEIDAO_CAN_TME_FLAG(mailbox) ((UINT32)(CAN_TSR_TME0 << (mailbox)))
#define FEIDAO_CAN_RQCP_FLAG(mailbox) ((UINT32)(0x38000000U | (1UL << ((mailbox) * 8U))))

enum FEIDAO_CAN_POWER_STATE
{
	FEIDAO_CAN_POWER_IDLE = 0,
	FEIDAO_CAN_POWER_WAIT_STABLE,
	FEIDAO_CAN_POWER_TX_WAIT,
	FEIDAO_CAN_POWER_RX_WINDOW
};

typedef struct
{
	UINT8 power_state;
	UINT8 tx_mailbox;
	UINT16 pending_mask;
	UINT32 power_tick;
	UINT32 tx_tick;
	UINT32 logical_tick;
	UINT32 last_hw_tick;
	UINT32 last_1000ms_tick;
	UINT32 last_5000ms_tick;
	UINT8 hw_tick_valid;
	UINT8 schedule_init;
	UINT8 bus_active;
	UINT8 no_ack_cnt;
	UINT8 probe_active;
	UINT8 rtc_service_active;
	UINT8 tx_cycle_acked;
	UINT8 tx_cycle_no_ack_recorded;
	UINT8 last_rtc_wake_tx_acked;
	UINT8 last_rtc_wake_timeout;
	UINT32 last_rtc_elapsed_seconds;
	UINT16 rtc_wake_service_cnt;
	UINT16 prepare_sleep_cnt;
} FeidaoCanRuntime;

static FeidaoCanRuntime s_feidao_can_runtime =
{
	FEIDAO_CAN_POWER_IDLE,
	CAN_TxStatus_NoMailBox,
	0U,
	0U,
	0U,
	0U,
	0U,
	0U,
	0U,
	0U,
	0U,
	1U,
	0U,
	0U,
	0U,
	0U,
	0U,
	0U,
	0U,
	0U,
	0U,
	0U,
};
static UINT32 s_u32FeidaoCanRxWindowTick = 0U;
static UINT8 s_u8FeidaoCanEnterIapDelayTicks = 0U;
static UINT16 s_u16FeidaoCanReadBlockWords[FEIDAO_CAN_APP_READ_BLOCK_MAX_WORDS];
static UINT8 s_u8FeidaoCanReadBlockCount = 0U;
static UINT8 s_u8FeidaoCanReadBlockIndex = 0U;
static UINT8 s_u8FeidaoCanReadBlockActive = 0U;
static UINT32 s_u32FeidaoCanReadBlockLastTick = 0U;
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
FeidaoCanRuntime * const g_dbg_feidao_can_runtime = &s_feidao_can_runtime;
#endif

#define s_u8FeidaoCanPowerState (s_feidao_can_runtime.power_state)
#define s_u8FeidaoCanTxMailbox (s_feidao_can_runtime.tx_mailbox)
#define s_u16FeidaoCanPendingMask (s_feidao_can_runtime.pending_mask)
#define s_u32FeidaoCanPowerTick (s_feidao_can_runtime.power_tick)
#define s_u32FeidaoCanTxTick (s_feidao_can_runtime.tx_tick)
#define s_u32FeidaoCanLogicalTick (s_feidao_can_runtime.logical_tick)
#define s_u32FeidaoCanLastHwTick (s_feidao_can_runtime.last_hw_tick)
#define s_u32FeidaoCanLast1000msTick (s_feidao_can_runtime.last_1000ms_tick)
#define s_u32FeidaoCanLast5000msTick (s_feidao_can_runtime.last_5000ms_tick)
#define s_u8FeidaoCanHwTickValid (s_feidao_can_runtime.hw_tick_valid)
#define s_u8FeidaoCanScheduleInit (s_feidao_can_runtime.schedule_init)
#define s_u8FeidaoCanBusActive (s_feidao_can_runtime.bus_active)
#define s_u8FeidaoCanNoAckCnt (s_feidao_can_runtime.no_ack_cnt)
#define s_u8FeidaoCanProbeActive (s_feidao_can_runtime.probe_active)
#define s_u8FeidaoCanRtcServiceActive (s_feidao_can_runtime.rtc_service_active)
#define s_u8FeidaoCanTxCycleAcked (s_feidao_can_runtime.tx_cycle_acked)
#define s_u8FeidaoCanTxCycleNoAckRecorded (s_feidao_can_runtime.tx_cycle_no_ack_recorded)
#define s_u8FeidaoCanLastRtcWakeTxAcked (s_feidao_can_runtime.last_rtc_wake_tx_acked)
#define s_u8FeidaoCanLastRtcWakeTimeout (s_feidao_can_runtime.last_rtc_wake_timeout)
#define s_u32FeidaoCanLastRtcElapsedSeconds (s_feidao_can_runtime.last_rtc_elapsed_seconds)
#define s_u16FeidaoCanRtcWakeServiceCnt (s_feidao_can_runtime.rtc_wake_service_cnt)
#define s_u16FeidaoCanPrepareSleepCnt (s_feidao_can_runtime.prepare_sleep_cnt)
static void Can_BusOFF_Monitor(void);
static UINT8 feidao_can_tick_elapsed(UINT32 now_tick, UINT32 start_tick, UINT32 wait_ticks);
static UINT32 feidao_can_seconds_to_ticks(UINT32 seconds);
static UINT32 feidao_can_update_logical_tick(UINT32 hw_tick);
static void feidao_can_invalidate_hw_tick(void);
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
static void feidao_can_update_low_power_status(void);
#else
#define feidao_can_update_low_power_status() do { } while (0)
#endif
static void feidao_can_power_on(UINT32 now_tick);
static void feidao_can_power_off(void);
static void feidao_can_open_rx_window(UINT32 now_tick);
static void feidao_can_abort_all_tx(void);
static void feidao_can_clear_tx_done(UINT8 mailbox);
static void feidao_can_mark_bus_active(void);
static void feidao_can_mark_no_ack(void);
static void feidao_can_begin_tx_cycle(void);
static void feidao_can_mark_tx_cycle_active(void);
static void feidao_can_record_tx_cycle_no_ack(void);
static UINT8 feidao_can_stop_cycle_after_no_ack(void);
static void feidao_can_drop_pending_if_bus_inactive(void);
static void feidao_can_anchor_schedule(UINT32 now_tick);
static void feidao_can_start_idle_probe(void);
static void feidao_can_schedule_rtc_period_frames(UINT32 now_tick, UINT32 elapsed_seconds);
static void feidao_can_inc_u16(volatile UINT16 *counter);
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
static void feidao_can_update_error_snapshot(void);
static void feidao_can_record_ack_error(void);
#else
#define feidao_can_update_error_snapshot() do { } while (0)
#define feidao_can_record_ack_error() do { } while (0)
#endif
static void feidao_can_record_tx_failed(void);
static void feidao_can_record_tx_timeout(void);
static void feidao_can_record_tx_no_mailbox(void);
static UINT8 feidao_can_mailbox_is_empty(UINT8 mailbox);
static void feidao_can_cancel_tx(UINT8 mailbox);
static void feidao_can_start_next_tx_or_power_off(UINT32 now_tick);
static void feidao_can_schedule_period_frames(UINT32 now_tick);
static void feidao_can_send(UINT32 now_tick);
static UINT8 feidao_can_service_until_idle(UINT32 timeout_ticks);
static void feidao_can_clear_sleep_runtime(void);
static void feidao_can_begin_rtc_wake_service(UINT32 elapsed_seconds);
static void feidao_can_queue_rtc_wake_frames(UINT8 was_bus_active, UINT32 elapsed_seconds);
static void feidao_can_finish_rtc_wake_service(UINT8 was_bus_active);
static void feidao_can_run_10ms_tasks(void);
static void feidao_can_service_enter_iap_delay(void);
static void feidao_can_handle_rx_msg(const CanRxMsg *rx_msg);
static UINT8 feidao_can_tick_elapsed(UINT32 now_tick, UINT32 start_tick, UINT32 wait_ticks)
{
	return (((UINT32)(now_tick - start_tick)) >= wait_ticks) ? 1U : 0U;
}

static UINT32 feidao_can_seconds_to_ticks(UINT32 seconds)
{
	if (seconds > ((UINT32)0xFFFFFFFFU / FEIDAO_CAN_TICKS_PER_SECOND))
	{
		return (UINT32)0xFFFFFFFFU;
	}

	return seconds * FEIDAO_CAN_TICKS_PER_SECOND;
}

static UINT32 feidao_can_update_logical_tick(UINT32 hw_tick)
{
	UINT32 delta;

	if (0U == s_u8FeidaoCanHwTickValid)
	{
		s_u8FeidaoCanHwTickValid = 1U;
		s_u32FeidaoCanLastHwTick = hw_tick;
		return s_u32FeidaoCanLogicalTick;
	}

	if (hw_tick >= s_u32FeidaoCanLastHwTick)
	{
		delta = hw_tick - s_u32FeidaoCanLastHwTick;
	}
	else
	{
		delta = hw_tick;
	}

	s_u32FeidaoCanLastHwTick = hw_tick;
	s_u32FeidaoCanLogicalTick += delta;
	return s_u32FeidaoCanLogicalTick;
}

static void feidao_can_invalidate_hw_tick(void)
{
	s_u8FeidaoCanHwTickValid = 0U;
}

#if PROJECT_CFG_DEBUG_WATCH_ENABLE
static void feidao_can_update_low_power_status(void)
{
	g_stCanLowPowerStatus.u8PowerState = s_u8FeidaoCanPowerState;
	g_stCanLowPowerStatus.u8BusActive = s_u8FeidaoCanBusActive;
	g_stCanLowPowerStatus.u8NoAckCnt = s_u8FeidaoCanNoAckCnt;
	g_stCanLowPowerStatus.u8ProbeActive = s_u8FeidaoCanProbeActive;
	g_stCanLowPowerStatus.u8TxMailbox = s_u8FeidaoCanTxMailbox;
	g_stCanLowPowerStatus.u8RtcServiceActive = s_u8FeidaoCanRtcServiceActive;
	g_stCanLowPowerStatus.u8LastRtcWakeTxAcked = s_u8FeidaoCanLastRtcWakeTxAcked;
	g_stCanLowPowerStatus.u8LastRtcWakeTimeout = s_u8FeidaoCanLastRtcWakeTimeout;
	g_stCanLowPowerStatus.u16PendingMask = s_u16FeidaoCanPendingMask;
	g_stCanLowPowerStatus.u16RtcWakeServiceCnt = s_u16FeidaoCanRtcWakeServiceCnt;
	g_stCanLowPowerStatus.u16PrepareSleepCnt = s_u16FeidaoCanPrepareSleepCnt;
	g_stCanLowPowerStatus.u32LogicalTick = s_u32FeidaoCanLogicalTick;
	g_stCanLowPowerStatus.u32LastRtcElapsedSeconds = s_u32FeidaoCanLastRtcElapsedSeconds;
}
#endif

static void feidao_can_clear_sleep_runtime(void)
{
	s_u8FeidaoCanProbeActive = 0U;
	s_u16FeidaoCanPendingMask = 0U;
	feidao_can_abort_all_tx();
	if ((CAN1->TSR & CAN_TSR_TME) != CAN_TSR_TME)
	{
		CAN_DeInit(CAN1);
	}
	feidao_can_power_off();
	feidao_can_invalidate_hw_tick();
}

static void feidao_can_begin_rtc_wake_service(UINT32 elapsed_seconds)
{
	s_u32FeidaoCanLogicalTick += feidao_can_seconds_to_ticks(elapsed_seconds);
	s_u32FeidaoCanLastRtcElapsedSeconds = elapsed_seconds;
	feidao_can_inc_u16(&s_u16FeidaoCanRtcWakeServiceCnt);
	feidao_can_invalidate_hw_tick();
	InitCan();
	s_u8FeidaoCanRtcServiceActive = 1U;
	s_u8FeidaoCanLastRtcWakeTxAcked = 0U;
	s_u8FeidaoCanLastRtcWakeTimeout = 0U;
	feidao_can_update_low_power_status();
}

static void feidao_can_queue_rtc_wake_frames(UINT8 was_bus_active, UINT32 elapsed_seconds)
{
	if (was_bus_active != 0U)
	{
		feidao_can_schedule_rtc_period_frames(s_u32FeidaoCanLogicalTick, elapsed_seconds);
	}
	else
	{
		feidao_can_start_idle_probe();
	}
}

static void feidao_can_finish_rtc_wake_service(UINT8 was_bus_active)
{
	if ((was_bus_active == 0U) && (s_u8FeidaoCanBusActive != 0U))
	{
		feidao_can_anchor_schedule(s_u32FeidaoCanLogicalTick);
	}
	else if (s_u8FeidaoCanBusActive == 0U)
	{
		s_u16FeidaoCanPendingMask = 0U;
	}

	Can_PrepareSleep();
	s_u8FeidaoCanRtcServiceActive = 0U;
	feidao_can_update_low_power_status();
}

static void feidao_can_run_10ms_tasks(void)
{
	if (g_st_SysTimeFlag.bits.b1Sys10msFlag)
	{
		Can_BusOFF_Monitor();
	}
}

static void feidao_can_mark_bus_active(void)
{
	s_u8FeidaoCanBusActive = 1U;
	s_u8FeidaoCanNoAckCnt = 0U;
	s_u8FeidaoCanTxCycleAcked = 1U;
}

static void feidao_can_mark_no_ack(void)
{
	if (s_u8FeidaoCanNoAckCnt < FEIDAO_CAN_NO_ACK_INACTIVE_LIMIT)
	{
		s_u8FeidaoCanNoAckCnt++;
	}

	if (s_u8FeidaoCanNoAckCnt >= FEIDAO_CAN_NO_ACK_INACTIVE_LIMIT)
	{
		s_u8FeidaoCanBusActive = 0U;
	}
}

static void feidao_can_begin_tx_cycle(void)
{
	s_u8FeidaoCanTxCycleAcked = 0U;
	s_u8FeidaoCanTxCycleNoAckRecorded = 0U;
}

static void feidao_can_mark_tx_cycle_active(void)
{
	s_u8FeidaoCanTxCycleAcked = 1U;
	if (s_u8FeidaoCanRtcServiceActive != 0U)
	{
		s_u8FeidaoCanLastRtcWakeTxAcked = 1U;
	}
	feidao_can_mark_bus_active();
}

static void feidao_can_record_tx_cycle_no_ack(void)
{
	if ((0U == s_u8FeidaoCanTxCycleAcked) && (0U == s_u8FeidaoCanTxCycleNoAckRecorded))
	{
		s_u8FeidaoCanTxCycleNoAckRecorded = 1U;
		feidao_can_mark_no_ack();
	}
}

static UINT8 feidao_can_stop_cycle_after_no_ack(void)
{
	if ((0U == s_u8FeidaoCanProbeActive) &&
		(0U == s_u8FeidaoCanTxCycleAcked) &&
		(0U != s_u8FeidaoCanTxCycleNoAckRecorded))
	{
		s_u16FeidaoCanPendingMask = 0U;
		feidao_can_power_off();
		return 1U;
	}

	return 0U;
}

static void feidao_can_drop_pending_if_bus_inactive(void)
{
	if ((0U == s_u8FeidaoCanBusActive) &&
		(s_u8FeidaoCanNoAckCnt >= FEIDAO_CAN_NO_ACK_INACTIVE_LIMIT) &&
		(0U == s_u8FeidaoCanProbeActive))
	{
		s_u16FeidaoCanPendingMask = 0U;
	}
}

static void feidao_can_anchor_schedule(UINT32 now_tick)
{
	s_u8FeidaoCanScheduleInit = 1U;
	s_u32FeidaoCanLast1000msTick = now_tick;
	s_u32FeidaoCanLast5000msTick = now_tick;
}

static void feidao_can_start_idle_probe(void)
{
	s_u16FeidaoCanPendingMask = CAN_FEIDAO_RTC_PROBE_MSG_MASK;
	s_u8FeidaoCanProbeActive = 1U;
}

static void feidao_can_schedule_rtc_period_frames(UINT32 now_tick, UINT32 elapsed_seconds)
{
	UINT32 elapsed_ticks = feidao_can_seconds_to_ticks(elapsed_seconds);

	if ((0U == s_u8FeidaoCanScheduleInit) && (elapsed_ticks > 0U))
	{
		s_u8FeidaoCanScheduleInit = 1U;
		s_u32FeidaoCanLast1000msTick = now_tick - elapsed_ticks;
		s_u32FeidaoCanLast5000msTick = now_tick - elapsed_ticks;
	}

	feidao_can_schedule_period_frames(now_tick);
}

static void feidao_can_inc_u16(volatile UINT16 *counter)
{
	if (*counter < (UINT16)0xFFFFU)
	{
		(*counter)++;
	}
}

#if PROJECT_CFG_DEBUG_WATCH_ENABLE
static void feidao_can_update_error_snapshot(void)
{
	UINT32 esr = CAN1->ESR;

	g_stCanErrorSnapshot.u8LastErrorCode = (UINT8)(esr & CAN_ESR_LEC);
	g_stCanErrorSnapshot.u8ReceiveErrorCounter = (UINT8)((esr & CAN_ESR_REC) >> 24);
	g_stCanErrorSnapshot.u8TransmitErrorCounter = (UINT8)((esr & CAN_ESR_TEC) >> 16);
	g_stCanErrorSnapshot.u8ErrorWarning = (UINT8)((esr & CAN_ESR_EWGF) ? 1U : 0U);
	g_stCanErrorSnapshot.u8ErrorPassive = (UINT8)((esr & CAN_ESR_EPVF) ? 1U : 0U);
	g_stCanErrorSnapshot.u8BusOff = (UINT8)((esr & CAN_ESR_BOFF) ? 1U : 0U);
}

static void feidao_can_record_ack_error(void)
{
	if (CAN_ErrorCode_ACKErr == g_stCanErrorSnapshot.u8LastErrorCode)
	{
		FEIDAO_CAN_ERROR_INC(u16AckErrorCnt);
	}
}
#endif

static void feidao_can_record_tx_failed(void)
{
	feidao_can_update_error_snapshot();
	FEIDAO_CAN_ERROR_INC(u16TxFailedCnt);
	feidao_can_record_ack_error();
	feidao_can_record_tx_cycle_no_ack();
}

static void feidao_can_record_tx_timeout(void)
{
	feidao_can_update_error_snapshot();
	FEIDAO_CAN_ERROR_INC(u16TxTimeoutCnt);
	feidao_can_record_tx_cycle_no_ack();
}

static void feidao_can_record_tx_no_mailbox(void)
{
	feidao_can_update_error_snapshot();
	FEIDAO_CAN_ERROR_INC(u16TxNoMailboxCnt);
}

static UINT8 feidao_can_mailbox_is_empty(UINT8 mailbox)
{
	if (mailbox >= FEIDAO_CAN_TXMAILBOX_COUNT)
	{
		return 1U;
	}

	return ((CAN1->TSR & FEIDAO_CAN_TME_FLAG(mailbox)) != 0U) ? 1U : 0U;
}

static void feidao_can_power_on(UINT32 now_tick)
{
	GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_ON_LEVEL);
	s_u32FeidaoCanPowerTick = now_tick;
	s_u8FeidaoCanTxMailbox = CAN_TxStatus_NoMailBox;
	feidao_can_begin_tx_cycle();
	s_u8FeidaoCanPowerState = FEIDAO_CAN_POWER_WAIT_STABLE;
}

static void feidao_can_power_off(void)
{
	GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_OFF_LEVEL);
	s_u8FeidaoCanTxMailbox = CAN_TxStatus_NoMailBox;
	s_u8FeidaoCanProbeActive = 0U;
	s_u8FeidaoCanTxCycleAcked = 0U;
	s_u8FeidaoCanTxCycleNoAckRecorded = 0U;
	s_u8FeidaoCanPowerState = FEIDAO_CAN_POWER_IDLE;
}

static void feidao_can_open_rx_window(UINT32 now_tick)
{
	GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_ON_LEVEL);
	s_u32FeidaoCanRxWindowTick = now_tick;
	s_u8FeidaoCanTxMailbox = CAN_TxStatus_NoMailBox;
	s_u8FeidaoCanPowerState = FEIDAO_CAN_POWER_RX_WINDOW;
}

static void feidao_can_abort_all_tx(void)
{
	UINT8 mailbox;

	for (mailbox = 0U; mailbox < FEIDAO_CAN_TXMAILBOX_COUNT; ++mailbox)
	{
		feidao_can_cancel_tx(mailbox);
	}
}

static void feidao_can_clear_tx_done(UINT8 mailbox)
{
	if (mailbox < FEIDAO_CAN_TXMAILBOX_COUNT)
	{
		CAN_ClearFlag(CAN1, FEIDAO_CAN_RQCP_FLAG(mailbox));
	}
}

static void feidao_can_cancel_tx(UINT8 mailbox)
{
	UINT16 wait_cnt = 0U;

	if (mailbox >= FEIDAO_CAN_TXMAILBOX_COUNT)
	{
		return;
	}

	if (!feidao_can_mailbox_is_empty(mailbox))
	{
		CAN_CancelTransmit(CAN1, mailbox);
		while ((!feidao_can_mailbox_is_empty(mailbox)) && (wait_cnt < FEIDAO_CAN_ABORT_WAIT_LOOP))
		{
			wait_cnt++;
		}
		FEIDAO_CAN_ERROR_INC(u16TxAbortCnt);
	}

	feidao_can_clear_tx_done(mailbox);
}

static void feidao_can_start_next_tx_or_power_off(UINT32 now_tick)
{
	if (0U == s_u16FeidaoCanPendingMask)
	{
		feidao_can_open_rx_window(now_tick);
		return;
	}

	feidao_can_drop_pending_if_bus_inactive();
	if (0U == s_u16FeidaoCanPendingMask)
	{
		feidao_can_power_off();
		return;
	}

	s_u8FeidaoCanTxMailbox = CanFeidao_SendNextPending(&s_u16FeidaoCanPendingMask);
	if (CAN_TxStatus_NoMailBox == s_u8FeidaoCanTxMailbox)
	{
		feidao_can_abort_all_tx();
		s_u16FeidaoCanPendingMask = 0U;
		feidao_can_power_off();
		return;
	}

	s_u32FeidaoCanTxTick = now_tick;
	s_u8FeidaoCanPowerState = FEIDAO_CAN_POWER_TX_WAIT;
}

static void feidao_can_schedule_period_frames(UINT32 now_tick)
{
	if (0U == s_u8FeidaoCanScheduleInit)
	{
		feidao_can_anchor_schedule(now_tick);
		return;
	}

	if ((0U == s_u8FeidaoCanBusActive) && (s_u8FeidaoCanNoAckCnt >= FEIDAO_CAN_NO_ACK_INACTIVE_LIMIT))
	{
		if (feidao_can_tick_elapsed(now_tick, s_u32FeidaoCanLast1000msTick, FEIDAO_CAN_PERIOD_1000MS_TICKS))
		{
			s_u32FeidaoCanLast1000msTick = now_tick;
			s_u16FeidaoCanPendingMask |= CAN_FEIDAO_RTC_PROBE_MSG_MASK;
			s_u8FeidaoCanProbeActive = 1U;
		}
		return;
	}

	if (feidao_can_tick_elapsed(now_tick, s_u32FeidaoCanLast1000msTick, FEIDAO_CAN_PERIOD_1000MS_TICKS))
	{
		s_u32FeidaoCanLast1000msTick = now_tick;
		s_u16FeidaoCanPendingMask |= CAN_FEIDAO_1000MS_MSG_MASK;
	}

	if (feidao_can_tick_elapsed(now_tick, s_u32FeidaoCanLast5000msTick, FEIDAO_CAN_PERIOD_5000MS_TICKS))
	{
		s_u32FeidaoCanLast5000msTick = now_tick;
		s_u16FeidaoCanPendingMask |= CAN_FEIDAO_5000MS_MSG_MASK;
	}
}

static void feidao_can_send(UINT32 now_tick)
{
	UINT8 tx_status;

	if ((0U == s_u8FeidaoCanProbeActive) && (0U == s_u8FeidaoCanRtcServiceActive))
	{
		feidao_can_schedule_period_frames(now_tick);
	}
	feidao_can_drop_pending_if_bus_inactive();

	if ((0U == s_u16FeidaoCanPendingMask) && (FEIDAO_CAN_POWER_IDLE == s_u8FeidaoCanPowerState))
	{
		return;
	}

	switch (s_u8FeidaoCanPowerState)
	{
	case FEIDAO_CAN_POWER_IDLE:
		if (s_u16FeidaoCanPendingMask)
		{
			feidao_can_power_on(now_tick);
		}
		break;

	case FEIDAO_CAN_POWER_WAIT_STABLE:
		if (feidao_can_tick_elapsed(now_tick, s_u32FeidaoCanPowerTick, FEIDAO_CAN_POWER_STABLE_TICKS))
		{
			feidao_can_start_next_tx_or_power_off(now_tick);
		}
		break;

	case FEIDAO_CAN_POWER_TX_WAIT:
		if (CAN_TxStatus_NoMailBox == s_u8FeidaoCanTxMailbox)
		{
			feidao_can_start_next_tx_or_power_off(now_tick);
			break;
		}

		tx_status = CAN_TransmitStatus(CAN1, s_u8FeidaoCanTxMailbox);
		if (CAN_TxStatus_Ok == tx_status)
		{
			feidao_can_mark_tx_cycle_active();
			feidao_can_clear_tx_done(s_u8FeidaoCanTxMailbox);
			s_u8FeidaoCanTxMailbox = CAN_TxStatus_NoMailBox;
			feidao_can_start_next_tx_or_power_off(now_tick);
		}
		else if (CAN_TxStatus_Failed == tx_status)
		{
			feidao_can_record_tx_failed();
			feidao_can_clear_tx_done(s_u8FeidaoCanTxMailbox);
			s_u8FeidaoCanTxMailbox = CAN_TxStatus_NoMailBox;
			if (feidao_can_stop_cycle_after_no_ack())
			{
				break;
			}
			feidao_can_start_next_tx_or_power_off(now_tick);
		}
		else if (feidao_can_tick_elapsed(now_tick, s_u32FeidaoCanTxTick, FEIDAO_CAN_TX_DONE_TIMEOUT_TICKS))
		{
			feidao_can_record_tx_timeout();
			feidao_can_cancel_tx(s_u8FeidaoCanTxMailbox);
			s_u8FeidaoCanTxMailbox = CAN_TxStatus_NoMailBox;
			if (feidao_can_stop_cycle_after_no_ack())
			{
				break;
			}
			feidao_can_start_next_tx_or_power_off(now_tick);
		}
		break;

	case FEIDAO_CAN_POWER_RX_WINDOW:
		if (feidao_can_tick_elapsed(now_tick, s_u32FeidaoCanRxWindowTick, FEIDAO_CAN_APP_RX_WINDOW_TICKS))
		{
			feidao_can_power_off();
		}
		break;

	default:
		s_u8FeidaoCanPowerState = FEIDAO_CAN_POWER_IDLE;
		break;
	}
}

static UINT8 feidao_can_service_until_idle(UINT32 timeout_ticks)
{
	UINT32 waited_ticks = 0U;

	while (Can_IsBusy() && (waited_ticks < timeout_ticks))
	{
		Feed_IWatchDog;
		__delay_ms(10);
		s_u32FeidaoCanLogicalTick++;
		feidao_can_send(s_u32FeidaoCanLogicalTick);
		waited_ticks++;
	}

	return Can_IsBusy() ? 0U : 1U;
}

UINT8 Can_HDX_Transmit(CanTxMsg *Msg)
{
	UINT8 u8MailBoxUsed;

	if (CAN_ID_STD == Msg->IDE)
	{
		Msg->StdId += ((UINT32)CAN_ADRESS_STD_ID << 7); // 单机版地址默认为0
	}

	u8MailBoxUsed = CAN_Transmit(CAN1, Msg);
	if (CAN_TxStatus_NoMailBox == u8MailBoxUsed)
	{
		feidao_can_record_tx_no_mailbox();
		System_ERROR_UserCallback(ERROR_CAN);
	}

	return u8MailBoxUsed;
}

static void InitCan_GPIO(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE); // 复用功能和GPIO端口时钟使能
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

	GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_OFF_LEVEL);
	GPIO_InitStructure.GPIO_Pin = PIN_CMNT_EN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIO_CMNT_EN, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;	  // Configure CAN pin: RX    // PD0
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 上拉输入
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;		// Configure CAN pin: TX    // PD1
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 复用推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// GPIO_PinRemapConfig(GPIO_Remap2_CAN1, ENABLE);	 	//14:13位  //0：映射到PA11，PA12(默认)//1: 不用。
}

static void InitCan_NVIC(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	/*不用设置了，systemInit()已经做了相应操作
	#ifdef  VECT_TAB_RAM
	  NVIC_SetVectorTable(NVIC_VectTab_RAM, 0x0);
	#else
	  NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0);
	#endif
	*/
	/* enabling interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

static void InitCan_Filter(void)
{
	UINT16 u16CAN_FilterIdHigh;
	UINT16 u16CAN_FilterIdLow;
	UINT16 u16CAN_FilterMaskIdHigh;
	UINT16 u16CAN_FilterMaskIdLow;
	CAN_FilterInitTypeDef CAN_FilterInitStructure;

	// 这两句怎么来的，详细看截图——过滤器配置表
	// CAN_ID_STD之类的不需要移位，官方已经定好
	// CAN_FilterMode_IdList，列表模式FBMx = 1
	// CAN_FilterScale_16bit，过滤器组的位宽，FSCx = 0
	// 结合这两个，高位的屏蔽位也作为标识符列表，可以搞4个CANID
	// 别的情况是高位作为低位的屏蔽位。

	// stm32 can的屏蔽位模式：
	// 一个是标识符寄存器(过滤器Filter)，一个是屏蔽位寄存器(Mask)。
	// 凡是屏蔽位寄存器里为1的位所对应的标识符寄存器的位，这些位是必须匹配的
	// 也就是说，你接受到的Message里面的标识符（ID）里面对应的位必须跟标识符寄存器里对应的位相同，才能被接受。

	// 远程帧过滤器
	u16CAN_FilterIdHigh = (CANID_RX_COMMON_MSG_MASK << 5) | CAN_ID_STD | CAN_RTR_DATA;
	u16CAN_FilterIdLow = (CANID_RX_COMMON_MSG_FILTER << 5) | CAN_ID_STD | CAN_RTR_DATA;

	u16CAN_FilterMaskIdHigh = (CANID_RX_COMMON_MSG_MASK << 5) | CAN_ID_STD | CAN_RTR_DATA; // 设置成一样
	u16CAN_FilterMaskIdLow = (CANID_RX_COMMON_MSG_FILTER << 5) | CAN_ID_STD | CAN_RTR_DATA;

	CAN_FilterInitStructure.CAN_FilterNumber = 0;							// 指定过滤器为0，如果想接收多几个，范围为0——13
	CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;			// 指定过滤器为屏蔽模式
	CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_16bit;		// 过滤器位宽为16位，也即2个带屏蔽位的标准帧
	CAN_FilterInitStructure.CAN_FilterIdHigh = u16CAN_FilterIdHigh;			// 过滤器标识符的高16位值
	CAN_FilterInitStructure.CAN_FilterIdLow = u16CAN_FilterIdLow;			// 过滤器标识符的低16位值
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = u16CAN_FilterMaskIdHigh; // 过滤器屏蔽标识符的高16位值
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = u16CAN_FilterMaskIdLow;	// 过滤器屏蔽标识符的低16位值
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;	// 设定了指向过滤器的FIFO为0
	CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;					// 使能过滤器
	CAN_FilterInit(&CAN_FilterInitStructure);								// 按上面的参数初始化过滤器
}

static void InitCan_CAN1(void)
{
	CAN_InitTypeDef CAN_InitStructure;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE); // CAN1 模块时钟使能
														 // APB1时钟最高36MHz

	CAN_DeInit(CAN1);					// 将外设CAN的全部寄存器重设为缺省值
	CAN_StructInit(&CAN_InitStructure); // 把CAN_InitStruct中的每一个参数按缺省值填入

	CAN_InitStructure.CAN_TTCM = DISABLE;		  // 没有使能时间触发模式
	CAN_InitStructure.CAN_ABOM = DISABLE;		  // 没有使能自动离线管理，BusOFF自动离线取消，需要手动处理
	CAN_InitStructure.CAN_AWUM = DISABLE;		  // 没有使能自动唤醒模式
	CAN_InitStructure.CAN_NART = ENABLE;           // no ACK时不自动重发，降低未接设备时的发送功耗
	CAN_InitStructure.CAN_RFLM = DISABLE;		  // 没有使能接收FIFO锁定模式
	CAN_InitStructure.CAN_TXFP = DISABLE;		  // 没有使能发送FIFO优先级
	CAN_InitStructure.CAN_Mode = CAN_Mode_Normal; // CAN设置为正常模式
												  // CAN_InitStructure.CAN_Mode = CAN_Mode_LoopBack;

	/*
	 * CAN bit timing target: 250 kbit/s.
	 * Current clock tree: PCLK1 = 8 MHz.
	 * bitrate = 8 MHz / 4 / (1 + 5 + 2) = 250 kbit/s.
	 * Keep this block aligned with the product CAN bus requirement.
	 */
	CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
	CAN_InitStructure.CAN_BS1 = CAN_BS1_5tq;
	CAN_InitStructure.CAN_BS2 = CAN_BS2_2tq;
	CAN_InitStructure.CAN_Prescaler = 4;
	CAN_Init(CAN1, &CAN_InitStructure);


	CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE); // 使能FIFO0消息挂号中断
}

static void Can_BusOFF_Monitor(void)
{
	UINT16 recover_cycle;
	static UINT8 s_u8BusOffRecoverStep = 0U;

	/* Detect the bus-off rising edge before running the old 10 ms counters. */
	if ((s_u8CanBusOff == 0U) && ((CAN1->ESR & CAN_ESR_BOFF) != 0U))
	{
		feidao_can_update_error_snapshot();
		FEIDAO_CAN_ERROR_INC(u16BusOffCnt);
		s_u8CanBusOff = 1U;
		CAN1->MCR |= CAN_MCR_INRQ;
		g_u16BusOff_RecoverCnt = 0U;
		g_u16BusOff_InitTestCnt = 0U;
		System_ERROR_UserCallback(ERROR_CAN);
	}

	/* Keep the original counter order: fault edge, counters, then recovery. */
	if (s_u8CanBusOff != 0U)
	{
		g_u16BusOff_InitTestCnt++;
	}
	if (((CAN1->ESR & CAN_ESR_BOFF) == 0U) && (s_u8CanBusOff == 0U))
	{
		g_u16BusOff_RecoverCnt++;
	}

	if (s_u8CanBusOff != 0U)
	{
		recover_cycle = (s_u8BusOffRecoverStep < 10U) ? DELAYB10MS_100MS : DELAYB10MS_1S;
		if (g_u16BusOff_InitTestCnt >= recover_cycle)
		{
			if (s_u8BusOffRecoverStep < 10U)
			{
				s_u8BusOffRecoverStep++;
			}
			g_u16BusOff_InitTestCnt = 0U;
			s_u8CanBusOff = 0U;
			CAN1->MCR &= ~CAN_MCR_INRQ;
		}
	}

	if (g_u16BusOff_RecoverCnt > DELAYB10MS_500MS)
	{
		g_u16BusOff_RecoverCnt = DELAYB10MS_500MS;
		s_u8BusOffRecoverStep = 0U;
	}
}

void InitCan(void)
{
	s_u8CanBusOff = 0U;
	s_u16FeidaoCanPendingMask = 0U;
	s_u8FeidaoCanTxMailbox = CAN_TxStatus_NoMailBox;
	s_u8FeidaoCanPowerState = FEIDAO_CAN_POWER_IDLE;
	InitCan_GPIO();
	InitCan_NVIC();
	InitCan_CAN1();
	InitCan_Filter();
	feidao_can_power_off();
	feidao_can_invalidate_hw_tick();
	feidao_can_update_low_power_status();
}

UINT8 Can_IsBusy(void)
{
	if (s_u8FeidaoCanPowerState != FEIDAO_CAN_POWER_IDLE)
	{
		return 1U;
	}
	if (s_u16FeidaoCanPendingMask != 0U)
	{
		return 1U;
	}
	if (s_u8FeidaoCanTxMailbox != CAN_TxStatus_NoMailBox)
	{
		return 1U;
	}
	if ((CAN1->TSR & CAN_TSR_TME) != CAN_TSR_TME)
	{
		return 1U;
	}

	return 0U;
}

void Can_PrepareSleep(void)
{
	feidao_can_inc_u16(&s_u16FeidaoCanPrepareSleepCnt);
	feidao_can_clear_sleep_runtime();
	feidao_can_update_low_power_status();
}

UINT8 Can_IsBusActive(void)
{
	return s_u8FeidaoCanBusActive;
}

UINT32 Can_GetIdleRtcPeriodSeconds(void)
{
	if (s_u8FeidaoCanBusActive != 0U)
	{
		return FEIDAO_CAN_RTC_ACTIVE_PERIOD_SECONDS;
	}

	return FEIDAO_CAN_RTC_IDLE_PERIOD_SECONDS;
}

void Can_RtcWakeService(UINT32 elapsed_seconds)
{
	UINT8 was_bus_active = s_u8FeidaoCanBusActive;
	UINT8 service_idle;

	feidao_can_begin_rtc_wake_service(elapsed_seconds);
	feidao_can_queue_rtc_wake_frames(was_bus_active, elapsed_seconds);
	feidao_can_send(s_u32FeidaoCanLogicalTick);
	service_idle = feidao_can_service_until_idle(FEIDAO_CAN_RTC_SERVICE_TIMEOUT_TICKS);
	if (service_idle == 0U)
	{
		s_u8FeidaoCanLastRtcWakeTimeout = 1U;
	}
	feidao_can_finish_rtc_wake_service(was_bus_active);
}

static UINT8 feidao_can_u16_to_percent(UINT16 value)
{
	if (value > 100U)
	{
		return 100U;
	}
	return (UINT8)value;
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

static void feidao_can_start_read_block_stream(UINT8 count)
{
	s_u8FeidaoCanReadBlockCount = count;
	s_u8FeidaoCanReadBlockIndex = 0U;
	s_u8FeidaoCanReadBlockActive = 1U;
	s_u32FeidaoCanReadBlockLastTick = s_u32FeidaoCanLogicalTick;
}

static void feidao_can_stop_read_block_stream(void)
{
	s_u8FeidaoCanReadBlockActive = 0U;
	s_u8FeidaoCanReadBlockCount = 0U;
	s_u8FeidaoCanReadBlockIndex = 0U;
}

static void feidao_can_service_read_block_stream(UINT32 now_tick)
{
	if (s_u8FeidaoCanReadBlockActive == 0U)
	{
		return;
	}
	if (s_u32FeidaoCanReadBlockLastTick == now_tick)
	{
		return;
	}
	s_u32FeidaoCanReadBlockLastTick = now_tick;

	if (s_u8FeidaoCanReadBlockIndex >= s_u8FeidaoCanReadBlockCount)
	{
		feidao_can_stop_read_block_stream();
		return;
	}

	feidao_can_app_send_word_frame(s_u8FeidaoCanReadBlockIndex,
								   s_u16FeidaoCanReadBlockWords[s_u8FeidaoCanReadBlockIndex]);
	s_u8FeidaoCanReadBlockIndex++;
	if (s_u8FeidaoCanReadBlockIndex >= s_u8FeidaoCanReadBlockCount)
	{
		feidao_can_stop_read_block_stream();
	}
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

static UINT8 feidao_can_take_app_cmd(UINT8 data[8])
{
	UINT8 has_cmd = 0U;

	__disable_irq();
	if (s_u8FeidaoCanAppCmdPending != 0U)
	{
		memcpy(data, s_u8FeidaoCanAppCmdData, 8U);
		s_u8FeidaoCanAppCmdPending = 0U;
		has_cmd = 1U;
	}
	__enable_irq();

	return has_cmd;
}

static void feidao_can_queue_app_cmd(const UINT8 data[8])
{
	if (s_u8FeidaoCanAppCmdPending == 0U)
	{
		memcpy(s_u8FeidaoCanAppCmdData, data, 8U);
		s_u8FeidaoCanAppCmdPending = 1U;
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
		s_u8FeidaoCanEnterIapDelayTicks = FEIDAO_CAN_APP_ENTER_IAP_DELAY_TICKS;
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
		host_error = Sci_HostReadWords(reg_addr, reg_count, s_u16FeidaoCanReadBlockWords);
		status = feidao_can_app_status_from_host_error(host_error);
		if (status == FEIDAO_CAN_APP_ACK_OK)
		{
			value0 = reg_count;
			value1 = 0U;
			feidao_can_start_read_block_stream(reg_count);
		}
		break;

	case FEIDAO_CAN_APP_CMD_WRITE_PREP:
		s_u16FeidaoCanWriteAddr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		s_u8FeidaoCanWriteValueHi = data[5];
		s_u8FeidaoCanWritePending = 1U;
		value0 = data[3];
		value1 = data[4];
		break;

	case FEIDAO_CAN_APP_CMD_WRITE_COMMIT:
		reg_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		if ((s_u8FeidaoCanWritePending == 0U) || (reg_addr != s_u16FeidaoCanWriteAddr))
		{
			s_u8FeidaoCanWritePending = 0U;
			status = FEIDAO_CAN_APP_ACK_BAD_PARAM;
			break;
		}
		reg_value = (UINT16)(((UINT16)s_u8FeidaoCanWriteValueHi << 8) | data[5]);
		s_u8FeidaoCanWritePending = 0U;
		host_error = Sci_HostWriteWords(reg_addr, &reg_value, 1U);
		status = feidao_can_app_status_from_host_error(host_error);
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

static void feidao_can_handle_rx_msg(const CanRxMsg *rx_msg)
{
	UINT16 expect_std_id = (UINT16)(((UINT16)CAN_ADRESS_STD_ID << 7) | FEIDAO_CAN_APP_CMD_ID);

	if ((rx_msg == 0) || (rx_msg->IDE != CAN_ID_STD))
	{
		return;
	}

	if ((UINT16)rx_msg->StdId == expect_std_id)
	{
		if (rx_msg->DLC == 8U)
		{
			feidao_can_queue_app_cmd(rx_msg->Data);
		}
	}
}

static void feidao_can_service_enter_iap_delay(void)
{
	if ((s_u8FeidaoCanEnterIapDelayTicks == 0U) || (0 == g_st_SysTimeFlag.bits.b1Sys10msFlag))
	{
		return;
	}

	s_u8FeidaoCanEnterIapDelayTicks--;
	if (s_u8FeidaoCanEnterIapDelayTicks == 0U)
	{
		u8FlashUpdateFlag = 1U;
	}
}

void App_Can(void)
{
	UINT32 now_tick = feidao_can_update_logical_tick(SysTime_Get10msTickCount());

	feidao_can_run_10ms_tasks();
	feidao_can_service_app_cmd();
	feidao_can_service_read_block_stream(now_tick);
	feidao_can_send(now_tick);
	feidao_can_service_enter_iap_delay();
	feidao_can_update_low_power_status();
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
	CanRxMsg rx_msg;

	sys_time.can_rcv_cnt++;
	CAN_Receive(CAN1, CAN_FIFO0, &rx_msg);
	feidao_can_mark_bus_active();
	feidao_can_handle_rx_msg(&rx_msg);
}
