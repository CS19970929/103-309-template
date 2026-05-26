#include "main.h"
#include "Flash.h"
#include "Sci_Upper.h"
#include "FactoryAging.h"
#include <string.h>

volatile union Can_Status Can_Status_Flag;
volatile union CanTxType_Status CanTxType_Flag;
CanTxMsg TxMessage;
CanRxMsg RxMessage;
volatile struct CAN_ERROR_SNAPSHOT g_stCanErrorSnapshot;

UINT16 g_u16BusOff_InitTestCnt = 0; // CAN总线关闭计时
UINT16 g_u16BusOff_RecoverCnt = 0;	// 5s计时标志位

#define FEIDAO_CAN_POWER_ON_LEVEL Bit_RESET
#define FEIDAO_CAN_POWER_OFF_LEVEL Bit_SET

#define FEIDAO_CAN_POWER_STABLE_TICKS ((UINT32)10U)     /* 100ms for transceiver wake */
#define FEIDAO_CAN_TX_DONE_TIMEOUT_TICKS ((UINT32)20U)  /* 200ms backstop after RTC wake */
#define FEIDAO_CAN_PERIOD_1000MS_TICKS ((UINT32)100U)
#define FEIDAO_CAN_PERIOD_5000MS_TICKS ((UINT32)500U)
#define FEIDAO_CAN_TICKS_PER_SECOND ((UINT32)100U)
#define FEIDAO_CAN_RTC_ACTIVE_PERIOD_SECONDS ((UINT32)1U)
#define FEIDAO_CAN_RTC_IDLE_PERIOD_SECONDS ((UINT32)10U)
#define FEIDAO_CAN_RTC_SERVICE_TIMEOUT_TICKS ((UINT32)150U)
#define FEIDAO_CAN_NO_ACK_INACTIVE_LIMIT ((UINT8)6U)
#define FEIDAO_CAN_MSG_VOLTAGE_CURRENT_1000MS ((UINT16)0x0001U)
#define FEIDAO_CAN_MSG_SOC_1000MS ((UINT16)0x0002U)
#define FEIDAO_CAN_MSG_CAP_5000MS ((UINT16)0x0004U)
#define FEIDAO_CAN_MSG_SOH_5000MS ((UINT16)0x0008U)
#define FEIDAO_CAN_MSG_VERSION_5000MS ((UINT16)0x0010U)
#define FEIDAO_CAN_MSG_STATUS_5000MS ((UINT16)0x0020U)
#define FEIDAO_CAN_MSG_FACTORY_TIME_5000MS ((UINT16)0x0040U)
#define FEIDAO_CAN_RTC_PROBE_MSG_MASK (FEIDAO_CAN_MSG_VOLTAGE_CURRENT_1000MS | FEIDAO_CAN_MSG_SOC_1000MS)
#define FEIDAO_CAN_TXMAILBOX_0 ((UINT8)0U)
#define FEIDAO_CAN_TXMAILBOX_1 ((UINT8)1U)
#define FEIDAO_CAN_TXMAILBOX_2 ((UINT8)2U)
#define FEIDAO_CAN_ABORT_WAIT_LOOP ((UINT16)1000U)
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
#define FEIDAO_CAN_APP_CMD_READ_BLOCK_DATA ((UINT8)0x86U)
#define FEIDAO_CAN_APP_READ_BLOCK_MAX_WORDS ((UINT8)120U)
#define FEIDAO_CAN_APP_READ_BLOCK_FRAME_INTERVAL_TICKS ((UINT32)1U)
#define FEIDAO_CAN_APP_ACK_OK ((UINT8)0x00U)
#define FEIDAO_CAN_APP_ACK_BAD_CMD ((UINT8)0x01U)
#define FEIDAO_CAN_APP_ACK_BAD_PARAM ((UINT8)0x02U)
#define FEIDAO_CAN_APP_ACK_FLASH_ERR ((UINT8)0x05U)
#define FEIDAO_CAN_APP_ACK_NO_PERMISSION ((UINT8)0x07U)
#define FEIDAO_CAN_APP_ACK_BMS_ERROR ((UINT8)0x08U)
#define FEIDAO_CAN_APP_ENTER_IAP_DELAY_TICKS ((UINT8)20U)
#define FEIDAO_CAN_APP_AGING_GUARD ((UINT8)0xA9U)
#define FEIDAO_CAN_APP_AGING_ACTION_START ((UINT8)0x51U)
#define FEIDAO_CAN_APP_AGING_ACTION_STOP ((UINT8)0x50U)
#define FEIDAO_CAN_APP_AGING_ACTION_RESET_TIME ((UINT8)0x5AU)

enum FEIDAO_CAN_POWER_STATE
{
	FEIDAO_CAN_POWER_IDLE = 0,
	FEIDAO_CAN_POWER_WAIT_STABLE,
	FEIDAO_CAN_POWER_TX_WAIT
};

static UINT8 s_u8FeidaoCanPowerState = FEIDAO_CAN_POWER_IDLE;
static UINT8 s_u8FeidaoCanTxMailbox = CAN_TxStatus_NoMailBox;
static UINT16 s_u16FeidaoCanPendingMask = 0U;
static UINT32 s_u32FeidaoCanPowerTick = 0U;
static UINT32 s_u32FeidaoCanTxTick = 0U;
static UINT32 s_u32FeidaoCanLogicalTick = 0U;
static UINT32 s_u32FeidaoCanLastHwTick = 0U;
static UINT32 s_u32FeidaoCanLast1000msTick = 0U;
static UINT32 s_u32FeidaoCanLast5000msTick = 0U;
static UINT8 s_u8FeidaoCanHwTickValid = 0U;
static UINT8 s_u8FeidaoCanScheduleInit = 0U;
static UINT8 s_u8FeidaoCanBusActive = 1U;
static UINT8 s_u8FeidaoCanNoAckCnt = 0U;
static UINT8 s_u8FeidaoCanProbeActive = 0U;
static UINT8 s_u8FeidaoCanRtcServiceActive = 0U;
static UINT8 s_u8FeidaoCanTxCycleAcked = 0U;
static UINT8 s_u8FeidaoCanTxCycleNoAckRecorded = 0U;
static volatile UINT8 s_u8AppCmdPending = 0U;
static UINT8 s_u8AppCmdData[8];
static UINT8 s_u8WritePending = 0U;
static UINT16 s_u16WriteAddr = 0U;
static UINT8 s_u8WriteValueHi = 0U;
static UINT8 s_u8EnterIapDelayTicks = 0U;
static UINT8 s_u8AppTxPending = 0U;
static UINT8 s_u8AppTxMailbox = CAN_TxStatus_NoMailBox;
static UINT32 s_u32AppTxTick = 0U;
static CanTxMsg s_stAppTxFrame;
static UINT16 s_u16ReadBlockWords[FEIDAO_CAN_APP_READ_BLOCK_MAX_WORDS];
static UINT8 s_u8ReadBlockCount = 0U;
static UINT8 s_u8ReadBlockIndex = 0U;
static UINT8 s_u8ReadBlockActive = 0U;
static UINT32 s_u32ReadBlockLastTick = 0U;
UINT8 CAN_Tx_Data(CanTxMsg *Msg);
static UINT8 feidao_can_tick_elapsed(UINT32 now_tick, UINT32 start_tick, UINT32 wait_ticks);
static UINT32 feidao_can_seconds_to_ticks(UINT32 seconds);
static UINT32 feidao_can_update_logical_tick(UINT32 hw_tick);
static void feidao_can_invalidate_hw_tick(void);
static void feidao_can_power_on(UINT32 now_tick);
static void feidao_can_power_off(void);
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
static void feidao_can_update_error_snapshot(void);
static void feidao_can_record_ack_error_from_snapshot(void);
static void feidao_can_record_tx_failed(void);
static void feidao_can_record_tx_timeout(void);
static void feidao_can_record_tx_no_mailbox(void);
static UINT8 feidao_can_mailbox_is_empty(UINT8 mailbox);
static void feidao_can_cancel_tx(UINT8 mailbox);
static UINT8 feidao_can_start_next_frame(void);
static void feidao_can_start_next_tx_or_power_off(UINT32 now_tick);
static void feidao_can_schedule_period_frames(UINT32 now_tick);
static void feidao_can_send(UINT32 now_tick);
static UINT8 feidao_can_service_until_idle(UINT32 timeout_ticks);
static UINT8 feidao_can_app_crc_ok(const UINT8 data[8]);
static void feidao_can_app_fill_crc(UINT8 data[8]);
static UINT8 feidao_can_u16_to_percent(UINT16 value);
static UINT8 feidao_can_aging_guard_ok(const UINT8 data[8], UINT8 action);
static UINT8 feidao_can_aging_remaining_hours(void);
static void feidao_can_fill_aging_ack(UINT8 *value0, UINT8 *value1);
static UINT8 feidao_can_app_status_from_host_error(UINT8 error);
static UINT8 feidao_can_request_iap(void);
static UINT8 feidao_can_take_app_cmd(UINT8 data[8]);
static void feidao_can_queue_app_cmd(const UINT8 data[8]);
static UINT8 feidao_can_app_queue_tx(const CanTxMsg *frame);
static void feidao_can_app_send_ack(UINT8 cmd, UINT8 status, UINT8 value0, UINT8 value1);
static void feidao_can_app_send_word_frame(UINT8 seq, UINT16 value);
static void feidao_can_service_app_tx(UINT32 now_tick);
static void feidao_can_start_read_block_stream(UINT8 count);
static void feidao_can_stop_read_block_stream(void);
static void feidao_can_service_read_block_stream(UINT32 now_tick);
static void feidao_can_handle_app_cmd_data(const UINT8 data[8]);
static void feidao_can_service_app_cmd(void);
static void feidao_can_service_enter_iap_delay(void);
static UINT8 feidao_can_handle_rx_msg(const CanRxMsg *rx_msg);
static void feidao_put_u16_be(uint8_t *data, uint8_t offset, uint16_t value)
{
	data[offset] = (uint8_t)((value >> 8) & 0xFF);
	data[offset + 1] = (uint8_t)(value & 0xFF);
}

static void feidao_put_u32_be(uint8_t *data, uint8_t offset, uint32_t value)
{
	data[offset] = (uint8_t)((value >> 24) & 0xFF);
	data[offset + 1] = (uint8_t)((value >> 16) & 0xFF);
	data[offset + 2] = (uint8_t)((value >> 8) & 0xFF);
	data[offset + 3] = (uint8_t)(value & 0xFF);
}

static void feidao_put_i32_be(uint8_t *data, uint8_t offset, int32_t value)
{
	feidao_put_u32_be(data, offset, (uint32_t)value);
}

UINT8 CAN_Battery_SendData_feidao(uint8_t chd_index, uint8_t *data, uint8_t length)
{
	CanTxMsg tx_msg;

	/* 设置CAN ID (扩展帧) */
	tx_msg.StdId = 0;
	tx_msg.RTR = CAN_RTR_DATA; // 为数据帧
	tx_msg.IDE = CAN_ID_EXT;
	tx_msg.ExtId = 0x14F80200 | chd_index;
	// tx_msg.ExtId = (BATTERY_CAN_ID << 24) | (BROADCAST_CAN_ID << 19) |
	//   (0x00 << 16) | (index << 8) | chd_index;

	/* 设置数据长度 */
	tx_msg.DLC = length;

	/* 设置数据 */
	for (uint8_t i = 0; i < length; i++)
	{
		tx_msg.Data[i] = data[i];
	}

	/* 发送CAN消息 */
	// CAN_Transmit(&tx_msg);
	return CAN_Tx_Data(&tx_msg);
}

UINT8 feidao_send_volage_current_1000ms(void)
{
	uint8_t data[8] = {0};
	int32_t current;
	uint32_t voltage = (uint32_t)g_stCellInfoReport.u16VCellTotle * 10;
	if (g_stCellInfoReport.u16IDischg > 0)
		current = -(uint32_t)g_stCellInfoReport.u16IDischg * 100;
	else
		current = (uint32_t)g_stCellInfoReport.u16Ichg * 100;

	// 实时电压（32位，MSB first）
	feidao_put_u32_be(data, 0, voltage);
	feidao_put_i32_be(data, 4, current);
	return CAN_Battery_SendData_feidao(0, data, 8);
}

UINT8 feidao_send_cap_5000ms(void)
{
	uint8_t data[8] = {0};
	// uint32_t cap = g_stCellInfoReport.SocElement.u16CapacityNow / 100 * 1000 * (3.6 * SNum);
	uint32_t real_cap = g_stCellInfoReport.SocElement.u16CapacityNow * 10 * g_stCellInfoReport.u16VCellTotle / 100;
	uint32_t design_cap = g_stCellInfoReport.SocElement.u16CapacityFactory * 10 * (36 * SNum) / 10;
	if (real_cap >= design_cap)
		real_cap = design_cap;

	feidao_put_u32_be(data, 0, real_cap);	// 实际容量，MSB first
	feidao_put_u32_be(data, 4, design_cap); // 设计容量，MSB first
	return CAN_Battery_SendData_feidao(1, data, 8);
}

UINT8 feidao_send_soc_1000ms(void)
{
	uint8_t data[8] = {0};
	uint8_t chg_status, soc;
	int8_t temp;
	uint8_t bat_type;
	uint16_t time_chg = 100;
	uint16_t res = 0;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp || g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp)
		chg_status = 2;
	else if (g_stCellInfoReport.u16Ichg)
		chg_status = 1;
	else
		chg_status = 0;
	soc = g_stCellInfoReport.SocElement.u16Soc;
	// soc = 66;
	// if(g_stCellInfoReport.u16TempMax < 400)
	temp = (int8_t)((int16_t)g_stCellInfoReport.u16TempMax / 10 - 40);

	data[0] = chg_status;
	data[1] = soc;
	data[2] = (uint8_t)temp;
	feidao_put_u16_be(data, 3, time_chg); // 剩余充电时间，MSB first

#if (BAT_TYPE == BAT_MASTER)
	bat_type = 0x00;
#elif (BAT_TYPE == BAT_SLAVE)
	bat_type = 0x01;
#endif // BAT_TYPE == BAT_MASTER
	data[5] = bat_type;
	feidao_put_u16_be(data, 6, res); // 保留字段，MSB first
	return CAN_Battery_SendData_feidao(2, data, 8);
}

UINT8 feidao_send_soh_5000ms(void)
{
	uint8_t data[8] = {0};
	uint8_t soh = g_stCellInfoReport.SocElement.u16Soh;
	uint16_t cycles = g_stCellInfoReport.SocElement.u16Cycle_times;

	data[0] = soh;
	feidao_put_u16_be(data, 1, cycles); // 循环次数，MSB first
	return CAN_Battery_SendData_feidao(3, data, 8);
}

UINT8 feidao_send_version_5000ms(void)
{
	uint8_t data[8] = {0};
	uint8_t pro_version = 1;
	uint16_t soft_version = 1;

	data[0] = pro_version;
	data[1] = soft_version;
	return CAN_Battery_SendData_feidao(4, data, 8);
}

UINT8 feidao_send_status_5000ms(void)
{
	uint8_t data[8] = {0};
	uint8_t work_status = 0;
	uint8_t exception_status = 0;
	uint16_t cap_fac, cap_now, cap_design;
	work_status |= work_status | (SystemStatus.bits.b1Status_MOS_DSG << 0);
	work_status |= work_status | (SystemStatus.bits.b1Status_MOS_CHG << 1);
	if (g_stCellInfoReport.u16Ichg)
	{
		work_status |= work_status | (1 << 2);
		//todo 
		work_status |= work_status | (1 << 3);
	}
	if (g_stCellInfoReport.u16IDischg)
		work_status |= work_status | (1 << 4);

	// exception_status |= exception_status | (g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp << 0);
	// exception_status |= exception_status | (g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp << 1);
	// exception_status |= exception_status | ((g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp | g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp) << 2);
	// exception_status |= exception_status | ((g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp | g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp) << 3);
	// exception_status |= exception_status | ((g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp | g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp | g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp) << 4);
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp)
		exception_status = 0x02;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp)
		exception_status = 0x03;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp)
		exception_status = 0x04;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp)
		exception_status = 0x05;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp || g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp)
		exception_status = 0x06;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp || g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp)
		exception_status = 0x07;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp)
		exception_status = 0x08;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp)
		exception_status = 0x09;
	if (System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG))
		exception_status = 0x0C;
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1VcellDeltaBig)
		exception_status = 0x0D;
	cap_fac = g_stCellInfoReport.SocElement.u16CapacityFactory * 10;
	cap_now = g_stCellInfoReport.SocElement.u16CapacityNow * 10;
	cap_design = g_stCellInfoReport.SocElement.u16CapacityFactory * 10;

	data[0] = work_status;
	data[1] = exception_status;
	feidao_put_u16_be(data, 2, cap_fac);	// 完全充电容量，MSB first
	feidao_put_u16_be(data, 4, cap_now);	// 当前剩余容量，MSB first
	feidao_put_u16_be(data, 6, cap_design); // 设计容量，MSB first
	return CAN_Battery_SendData_feidao(5, data, 8);
}

UINT8 feidao_send_factory_time_5000ms(void)
{
	uint8_t data[8] = {0};
	UINT32 aging_remaining_min;

	feidao_put_u16_be(data, 0, g_stCellInfoReport.SocElement.u16CapacityFactory * 10);
	data[2] = FactoryAging_GetState();
	aging_remaining_min = (FactoryAging_GetRemainingSeconds() + 59U) / 60U;
	if (aging_remaining_min > 0xFFFFU)
	{
		aging_remaining_min = 0xFFFFU;
	}
	feidao_put_u16_be(data, 3, (UINT16)aging_remaining_min);
	data[5] = FD_YEAR;
	data[6] = FD_MONTH;
	data[7] = FD_DAY;

	return CAN_Battery_SendData_feidao(8, data, 8);
}

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
	s_u16FeidaoCanPendingMask = FEIDAO_CAN_RTC_PROBE_MSG_MASK;
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

static void feidao_can_record_ack_error_from_snapshot(void)
{
	if (CAN_ErrorCode_ACKErr == g_stCanErrorSnapshot.u8LastErrorCode)
	{
		feidao_can_inc_u16(&g_stCanErrorSnapshot.u16AckErrorCnt);
	}
}

static void feidao_can_record_tx_failed(void)
{
	feidao_can_update_error_snapshot();
	feidao_can_inc_u16(&g_stCanErrorSnapshot.u16TxFailedCnt);
	feidao_can_record_ack_error_from_snapshot();
	feidao_can_record_tx_cycle_no_ack();
}

static void feidao_can_record_tx_timeout(void)
{
	feidao_can_update_error_snapshot();
	feidao_can_inc_u16(&g_stCanErrorSnapshot.u16TxTimeoutCnt);
	feidao_can_record_tx_cycle_no_ack();
}

static void feidao_can_record_tx_no_mailbox(void)
{
	feidao_can_update_error_snapshot();
	feidao_can_inc_u16(&g_stCanErrorSnapshot.u16TxNoMailboxCnt);
}

static UINT8 feidao_can_mailbox_is_empty(UINT8 mailbox)
{
	switch (mailbox)
	{
	case FEIDAO_CAN_TXMAILBOX_0:
		return ((CAN1->TSR & CAN_TSR_TME0) != 0U) ? 1U : 0U;
	case FEIDAO_CAN_TXMAILBOX_1:
		return ((CAN1->TSR & CAN_TSR_TME1) != 0U) ? 1U : 0U;
	case FEIDAO_CAN_TXMAILBOX_2:
		return ((CAN1->TSR & CAN_TSR_TME2) != 0U) ? 1U : 0U;
	default:
		return 1U;
	}
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

static void feidao_can_abort_all_tx(void)
{
	feidao_can_cancel_tx(FEIDAO_CAN_TXMAILBOX_0);
	feidao_can_cancel_tx(FEIDAO_CAN_TXMAILBOX_1);
	feidao_can_cancel_tx(FEIDAO_CAN_TXMAILBOX_2);
}

static void feidao_can_clear_tx_done(UINT8 mailbox)
{
	switch (mailbox)
	{
	case FEIDAO_CAN_TXMAILBOX_0:
		CAN_ClearFlag(CAN1, CAN_FLAG_RQCP0);
		break;
	case FEIDAO_CAN_TXMAILBOX_1:
		CAN_ClearFlag(CAN1, CAN_FLAG_RQCP1);
		break;
	case FEIDAO_CAN_TXMAILBOX_2:
		CAN_ClearFlag(CAN1, CAN_FLAG_RQCP2);
		break;
	default:
		break;
	}
}

static void feidao_can_cancel_tx(UINT8 mailbox)
{
	UINT16 wait_cnt = 0U;

	if (mailbox > FEIDAO_CAN_TXMAILBOX_2)
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
		feidao_can_inc_u16(&g_stCanErrorSnapshot.u16TxAbortCnt);
	}

	feidao_can_clear_tx_done(mailbox);
}

static UINT8 feidao_can_start_next_frame(void)
{
	if (s_u16FeidaoCanPendingMask & FEIDAO_CAN_MSG_VOLTAGE_CURRENT_1000MS)
	{
		s_u16FeidaoCanPendingMask &= (UINT16)(~FEIDAO_CAN_MSG_VOLTAGE_CURRENT_1000MS);
		return feidao_send_volage_current_1000ms();
	}
	if (s_u16FeidaoCanPendingMask & FEIDAO_CAN_MSG_SOC_1000MS)
	{
		s_u16FeidaoCanPendingMask &= (UINT16)(~FEIDAO_CAN_MSG_SOC_1000MS);
		return feidao_send_soc_1000ms();
	}
	if (s_u16FeidaoCanPendingMask & FEIDAO_CAN_MSG_CAP_5000MS)
	{
		s_u16FeidaoCanPendingMask &= (UINT16)(~FEIDAO_CAN_MSG_CAP_5000MS);
		return feidao_send_cap_5000ms();
	}
	if (s_u16FeidaoCanPendingMask & FEIDAO_CAN_MSG_SOH_5000MS)
	{
		s_u16FeidaoCanPendingMask &= (UINT16)(~FEIDAO_CAN_MSG_SOH_5000MS);
		return feidao_send_soh_5000ms();
	}
	if (s_u16FeidaoCanPendingMask & FEIDAO_CAN_MSG_VERSION_5000MS)
	{
		s_u16FeidaoCanPendingMask &= (UINT16)(~FEIDAO_CAN_MSG_VERSION_5000MS);
		return feidao_send_version_5000ms();
	}
	if (s_u16FeidaoCanPendingMask & FEIDAO_CAN_MSG_STATUS_5000MS)
	{
		s_u16FeidaoCanPendingMask &= (UINT16)(~FEIDAO_CAN_MSG_STATUS_5000MS);
		return feidao_send_status_5000ms();
	}
	if (s_u16FeidaoCanPendingMask & FEIDAO_CAN_MSG_FACTORY_TIME_5000MS)
	{
		s_u16FeidaoCanPendingMask &= (UINT16)(~FEIDAO_CAN_MSG_FACTORY_TIME_5000MS);
		return feidao_send_factory_time_5000ms();
	}

	return CAN_TxStatus_NoMailBox;
}

static void feidao_can_start_next_tx_or_power_off(UINT32 now_tick)
{
	if (0U == s_u16FeidaoCanPendingMask)
	{
		feidao_can_power_off();
		return;
	}

	feidao_can_drop_pending_if_bus_inactive();
	if (0U == s_u16FeidaoCanPendingMask)
	{
		feidao_can_power_off();
		return;
	}

	s_u8FeidaoCanTxMailbox = feidao_can_start_next_frame();
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
			s_u16FeidaoCanPendingMask |= FEIDAO_CAN_RTC_PROBE_MSG_MASK;
			s_u8FeidaoCanProbeActive = 1U;
		}
		return;
	}

	if (feidao_can_tick_elapsed(now_tick, s_u32FeidaoCanLast1000msTick, FEIDAO_CAN_PERIOD_1000MS_TICKS))
	{
		s_u32FeidaoCanLast1000msTick = now_tick;
		s_u16FeidaoCanPendingMask |= (FEIDAO_CAN_MSG_VOLTAGE_CURRENT_1000MS | FEIDAO_CAN_MSG_SOC_1000MS);
	}

	if (feidao_can_tick_elapsed(now_tick, s_u32FeidaoCanLast5000msTick, FEIDAO_CAN_PERIOD_5000MS_TICKS))
	{
		s_u32FeidaoCanLast5000msTick = now_tick;
		s_u16FeidaoCanPendingMask |= (FEIDAO_CAN_MSG_CAP_5000MS |
									  FEIDAO_CAN_MSG_SOH_5000MS |
									  FEIDAO_CAN_MSG_VERSION_5000MS |
									  FEIDAO_CAN_MSG_STATUS_5000MS |
									  FEIDAO_CAN_MSG_FACTORY_TIME_5000MS);
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
		feidao_can_service_read_block_stream(s_u32FeidaoCanLogicalTick);
		feidao_can_service_app_tx(s_u32FeidaoCanLogicalTick);
		waited_ticks++;
	}

	return Can_IsBusy() ? 0U : 1U;
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
			(data[5] == (UINT8)(FEIDAO_CAN_APP_AGING_GUARD ^ action))) ? 1U : 0U;
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

static UINT8 feidao_can_request_iap(void)
{
	return AppUpgrade_RequestIap();
}

static UINT8 feidao_can_take_app_cmd(UINT8 data[8])
{
	UINT8 has_cmd = 0U;

	__disable_irq();
	if (s_u8AppCmdPending != 0U)
	{
		memcpy(data, s_u8AppCmdData, 8U);
		s_u8AppCmdPending = 0U;
		has_cmd = 1U;
	}
	__enable_irq();

	return has_cmd;
}

static void feidao_can_queue_app_cmd(const UINT8 data[8])
{
	if (s_u8AppCmdPending == 0U)
	{
		memcpy(s_u8AppCmdData, data, 8U);
		s_u8AppCmdPending = 1U;
	}
}

static UINT8 feidao_can_app_queue_tx(const CanTxMsg *frame)
{
	if ((frame == 0) || (s_u8AppTxPending != 0U) || (s_u8AppTxMailbox != CAN_TxStatus_NoMailBox))
	{
		feidao_can_record_tx_no_mailbox();
		return 0U;
	}

	s_stAppTxFrame = *frame;
	s_u8AppTxPending = 1U;
	return 1U;
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
	(void)feidao_can_app_queue_tx(&tx_msg);
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
	(void)feidao_can_app_queue_tx(&tx_msg);
}

static void feidao_can_service_app_tx(UINT32 now_tick)
{
	UINT8 tx_status;

	if (s_u8AppTxMailbox != CAN_TxStatus_NoMailBox)
	{
		tx_status = CAN_TransmitStatus(CAN1, s_u8AppTxMailbox);
		if (CAN_TxStatus_Ok == tx_status)
		{
			feidao_can_mark_bus_active();
			feidao_can_clear_tx_done(s_u8AppTxMailbox);
			s_u8AppTxMailbox = CAN_TxStatus_NoMailBox;
		}
		else if (CAN_TxStatus_Failed == tx_status)
		{
			feidao_can_record_tx_failed();
			feidao_can_clear_tx_done(s_u8AppTxMailbox);
			s_u8AppTxMailbox = CAN_TxStatus_NoMailBox;
		}
		else if (feidao_can_tick_elapsed(now_tick, s_u32AppTxTick, FEIDAO_CAN_TX_DONE_TIMEOUT_TICKS))
		{
			feidao_can_record_tx_timeout();
			feidao_can_cancel_tx(s_u8AppTxMailbox);
			s_u8AppTxMailbox = CAN_TxStatus_NoMailBox;
		}
	}

	if ((s_u8AppTxMailbox == CAN_TxStatus_NoMailBox) && (s_u8AppTxPending != 0U))
	{
		GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, FEIDAO_CAN_POWER_ON_LEVEL);
		s_u8AppTxPending = 0U;
		s_u8AppTxMailbox = CAN_Tx_Data(&s_stAppTxFrame);
		s_u32AppTxTick = now_tick;
	}
}

static void feidao_can_start_read_block_stream(UINT8 count)
{
	s_u8ReadBlockCount = count;
	s_u8ReadBlockIndex = 0U;
	s_u8ReadBlockActive = 1U;
	s_u32ReadBlockLastTick = s_u32FeidaoCanLogicalTick - FEIDAO_CAN_APP_READ_BLOCK_FRAME_INTERVAL_TICKS;
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
	if ((s_u8AppTxPending != 0U) || (s_u8AppTxMailbox != CAN_TxStatus_NoMailBox))
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
		if (feidao_can_request_iap() == 0U)
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

static UINT8 feidao_can_handle_rx_msg(const CanRxMsg *rx_msg)
{
	UINT16 expect_std_id = (UINT16)(((UINT16)CAN_ADRESS_STD_ID << 7) | FEIDAO_CAN_APP_CMD_ID);

	if ((rx_msg == 0) || (rx_msg->IDE != CAN_ID_STD))
	{
		return 0U;
	}

	feidao_can_mark_bus_active();
	if (((UINT16)rx_msg->StdId == expect_std_id) && (rx_msg->DLC == 8U))
	{
		feidao_can_queue_app_cmd(rx_msg->Data);
		return 1U;
	}

	return 0U;
}

void CAN_TX_Test(void)
{
	UINT8 TXCounter = 0, TXStatus = 0;
	UINT8 u8MailBoxUsed;
	TxMessage.StdId = CANID_TX_Test; // 标准标识符
	TxMessage.ExtId = 0;			 // 扩展标识符
	TxMessage.IDE = CAN_ID_STD;		 // 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;	 // 为数据帧
	TxMessage.DLC = 8;				 // 消息的数据长度为8个字节
	TxMessage.Data[0] = 1;			 // 数据
	TxMessage.Data[1] = 2;
	TxMessage.Data[2] = 3;
	TxMessage.Data[3] = 4;
	TxMessage.Data[4] = 5;
	TxMessage.Data[5] = 6;
	TxMessage.Data[6] = 7;
	TxMessage.Data[7] = 8;

	/*
	while((CAN_TxStatus_Ok!=CAN_TransmitStatus(CAN1,CAN_Transmit(CAN1,&TxMessage)))&&(++i<0xFFF));		//发送数据
	if(i >= 0xFF) {
		ERROR_UserCallback(CAN_ERROR);
	}
	*/
	u8MailBoxUsed = CAN_Transmit(CAN1, &TxMessage);
	// CAN_Transmit已集成Can发送流程代码，非常方便
	// 这个++i和硬件的BusOFF是不冲突的，这个可以是在等邮箱空的次数，CAN_TxStatus_Pending和BusOFF分开
	do
	{
		// 出现Can错误的原因就是这句话！连续执行导致连续mailbox0是满的！CAN_Transmit()地方都放错了！
		// TXStatus = CAN_TransmitStatus(CAN1,CAN_Transmit(CAN1,&TxMessage));
		TXStatus = CAN_TransmitStatus(CAN1, u8MailBoxUsed);
		TXCounter++;
	} while ((TXStatus != CAN_TxStatus_Ok) && (TXCounter < 0xFF)); // Fail和OK不用管

	if (TXCounter >= 0xFF)
	{
		System_ERROR_UserCallback(ERROR_CAN); // 这里应该是一个Pending_Error但是Can模块不可能需要等这么久吧。
	}
}

UINT8 CAN_Tx_Data(CanTxMsg *Msg)
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
void CAN_TX_0x00(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x00; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCellTotle; // 总压，10mV
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	if (g_stCellInfoReport.u16IDischg > 0)
	{													 // 总电流，符号型，充电为正，放电为负
		u16_tmp16a = g_stCellInfoReport.u16IDischg * 10; // 10mA
		u16_tmp16a = (0x7FFF - u16_tmp16a + 1) | 0x8000;
	}
	else
	{
		u16_tmp16a = g_stCellInfoReport.u16Ichg * 10; // 电池总电流
	}
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.SocElement.u16CapacityNow; // 剩余容量10mAh
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x01(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x01; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.SocElement.u16CapacityFull; // 满电容量10mAh
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.SocElement.u16Cycle_times; // 循环次数
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.SocElement.u16Soc;
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x02(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x02; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16BalanceFlag1;
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16BalanceFlag2;
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.unMdlFault_Third.all;
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x03(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x03; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	switch (OPEN)
	{
	case 0:
		u16_tmp16a = ((~((UINT16)(SystemStatus.all & 0x000003FF))) & 0x00FE) | (((UINT16)(SystemStatus.all & 0x000003FF)) & 0xFF01);
		break;
	case 1:
		u16_tmp16a = (UINT16)(SystemStatus.all & 0x000003FF);
		break;
	default:
		u16_tmp16a = (UINT16)(SystemStatus.all & 0x000003FF);
		break;
	}
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = 0x1234; // 生产日期
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = 0x1234; // 软件版本
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x04(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x04; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	TxMessage.Data[0] = SeriesNum; // 电池串数
	TxMessage.Data[1] = System_ErrFlag.u8ErrFlag_CBC_DSG > 0 ? 1 : 0;

	u16_tmp16a = g_stCellInfoReport.u16TempMax;
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16TempMin;
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x05(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x05; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.SocElement.u16Soh;
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = 0;
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = 0;
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x06(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x06; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = 0;
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = 0;
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = 0;
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x07(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x07; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[0];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[1];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[2];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x08(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x08; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[3];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[4];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[5];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x09(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x09; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[6];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[7];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[8];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x0A(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x0A; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[9];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[10];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[11];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x0B(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x0B; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[12];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[13];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[14];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x0C(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x0C; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[15];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[16];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[17];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x0D(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x0D; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[18];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[19];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[20];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x0E(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x0E; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[21];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[22];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[23];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x0F(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x0F; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[24];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[25];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[26];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x10(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x10; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[27];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[28];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[29];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x11(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x11; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[30];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[31];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = 0;
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void InitCan_GPIO(void)
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

void InitCan_NVIC(void)
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

void InitCan_Filter(void)
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

void InitCan_CAN1(void)
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

void Can_BusOFF_FaultTimeCtrl(void)
{
	if (TRUE == Can_Status_Flag.bits.b1Can_BusOFF)
	{
		g_u16BusOff_InitTestCnt++; // BUSOFF计时
	}
	if ((FALSE == (CAN1->ESR & CAN_ESR_BOFF)) && (FALSE == Can_Status_Flag.bits.b1Can_BusOFF))
	{
		g_u16BusOff_RecoverCnt++; // BUSOFF清除计时
	}
}

void Can_BusOFF_FaultChk(void)
{
	static UINT8 s_u8FlagBusOff = 0;
	if (FALSE == Can_Status_Flag.bits.b1Can_BusOFF)
	{
		if (CAN1->ESR & CAN_ESR_BOFF)
		{											  // 检测到BusOff，总线进入离线状态，找不到相关函数，自己写
			feidao_can_update_error_snapshot();
			feidao_can_inc_u16(&g_stCanErrorSnapshot.u16BusOffCnt);
			Can_Status_Flag.bits.b1Can_BusOFF = TRUE; // 找到了 --> CAN_GetFlagStatus(CAN1, CAN_FLAG_BOF)==SET
			CAN1->MCR |= CAN_MCR_INRQ;				  // 置位，从正常模式转为初始化模式(一旦当前的CAN活动(发送或接收)结束，CAN就进入初始化模式)
			s_u8FlagBusOff = 1;
			g_u16BusOff_RecoverCnt = 0; // 时序计算初始化
			g_u16BusOff_InitTestCnt = 0;
			System_ERROR_UserCallback(ERROR_CAN);
		}
	}

	if (1 == s_u8FlagBusOff)
	{ // 下一轮过来置回环模式
		s_u8FlagBusOff = 0;
		// CAN1->BTR =	(UINT32)CAN_Mode_LoopBack<<30;    	//请求环回模式，不需要静默回环模式，收到需要发ACK到总线上。
		// 真的需要改为回环模式吗，打个问号？软件INRQ位处理可在初始化模式和正常模式切换
	}
}

void Can_BusOFF_Recover(void)
{
	UINT16 u16BusOFF_InitCycleT;
	static UINT8 s_u8BusOFF_InitCnt = 0;
	if (TRUE == Can_Status_Flag.bits.b1Can_BusOFF)
	{
		if (s_u8BusOFF_InitCnt < 10)
		{											 // 快恢复计时10次
			u16BusOFF_InitCycleT = DELAYB10MS_100MS; // 快恢复计时10次100ms
		}
		else
		{
			s_u8BusOFF_InitCnt = 10;
			u16BusOFF_InitCycleT = DELAYB10MS_1S; // 1s周期
		}

		if (g_u16BusOff_InitTestCnt >= u16BusOFF_InitCycleT)
		{ // 周期初始化CAN，前10次为100ms，后面为1s
			s_u8BusOFF_InitCnt++;
			g_u16BusOff_InitTestCnt = 0;
			Can_Status_Flag.bits.b1Can_BusOFF_TestSd = 1; // 时间到尝试发送Test报文
			Can_Status_Flag.bits.b1Can_BusOFF = FALSE;
			// CAN1->BTR =	(UINT32)CAN_Mode_Normal<<30;		//请求正常模式
			CAN1->MCR &= ~CAN_MCR_INRQ; // 复位，从初始化模式转为正常模式(当CAN在接收引脚检测到连续的11个隐性位后，CAN就达到同步)
		}
	}

	if (g_u16BusOff_RecoverCnt > DELAYB10MS_500MS)
	{ // 5S内未检测到BusOFF标志，则表示与外部通信恢复正常
		g_u16BusOff_RecoverCnt = DELAYB10MS_500MS;
		s_u8BusOFF_InitCnt = 0;
	}
}

void Can_BusOFF_Monitor(void)
{
	Can_BusOFF_FaultChk();
	Can_BusOFF_FaultTimeCtrl();
	Can_BusOFF_Recover();
}

void Can_ReceiveDeal(void)
{
	if (!Can_Status_Flag.bits.b1Can_Received)
	{
		return;
	}

	// ID已经过滤掉，不需要再判断ID是否为本机
	if ((RxMessage.StdId >> 7) != CAN_ADRESS_STD_ID)
	{
		return;
	}

	switch (RxMessage.StdId & 0x007F)
	{ // 按照逻辑，是这个函数处理接受数据，然后处理数据，然后赋值标志位返回才对
	case CANID_CHECK_0x00:
		CanTxType_Flag.bits.b1CanTx_0x00 = 1;
		break;
	case CANID_CHECK_0x01:
		CanTxType_Flag.bits.b1CanTx_0x01 = 1;
		break;
	case CANID_CHECK_0x02:
		CanTxType_Flag.bits.b1CanTx_0x02 = 1;
		break;
	case CANID_CHECK_0x03:
		CanTxType_Flag.bits.b1CanTx_0x03 = 1;
		break;
	case CANID_CHECK_0x04:
		CanTxType_Flag.bits.b1CanTx_0x04 = 1;
		break;
	case CANID_CHECK_0x05:
		CanTxType_Flag.bits.b1CanTx_0x05 = 1;
		break;
	case CANID_CHECK_0x06:
		CanTxType_Flag.bits.b1CanTx_0x06 = 1;
		break;
	case CANID_CHECK_0x07:
		CanTxType_Flag.bits.b1CanTx_0x07 = 1;
		break;
	case CANID_CHECK_0x08:
		CanTxType_Flag.bits.b1CanTx_0x08 = 1;
		break;
	case CANID_CHECK_0x09:
		CanTxType_Flag.bits.b1CanTx_0x09 = 1;
		break;
	case CANID_CHECK_0x0A:
		CanTxType_Flag.bits.b1CanTx_0x0A = 1;
		break;
	case CANID_CHECK_0x0B:
		CanTxType_Flag.bits.b1CanTx_0x0B = 1;
		break;
	case CANID_CHECK_0x0C:
		CanTxType_Flag.bits.b1CanTx_0x0C = 1;
		break;
	case CANID_CHECK_0x0D:
		CanTxType_Flag.bits.b1CanTx_0x0D = 1;
		break;
	case CANID_CHECK_0x0E:
		CanTxType_Flag.bits.b1CanTx_0x0E = 1;
		break;
	case CANID_CHECK_0x0F:
		CanTxType_Flag.bits.b1CanTx_0x0F = 1;
		break;
	case CANID_CHECK_0x10:
		CanTxType_Flag.bits.b1CanTx_0x10 = 1;
		break;
	case CANID_CHECK_0x11:
		CanTxType_Flag.bits.b1CanTx_0x11 = 1;
		break;

	default:
		break;
	}

	Can_Status_Flag.bits.b1Can_Received = 0;
}

void Can_TransmitDeal(void)
{
	if (CanTxType_Flag.all)
	{
		RTC_ExtComCnt++;
	}

	if (CanTxType_Flag.bits.b1CanTx_0x00)
	{
		CanTxType_Flag.bits.b1CanTx_0x00 = 0;
		CAN_TX_0x00();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x01)
	{
		CanTxType_Flag.bits.b1CanTx_0x01 = 0;
		CAN_TX_0x01();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x02)
	{
		CanTxType_Flag.bits.b1CanTx_0x02 = 0;
		CAN_TX_0x02();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x03)
	{
		CanTxType_Flag.bits.b1CanTx_0x03 = 0;
		CAN_TX_0x03();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x04)
	{
		CanTxType_Flag.bits.b1CanTx_0x04 = 0;
		CAN_TX_0x04();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x05)
	{
		CanTxType_Flag.bits.b1CanTx_0x05 = 0;
		CAN_TX_0x05();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x06)
	{
		CanTxType_Flag.bits.b1CanTx_0x06 = 0;
		CAN_TX_0x06();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x07)
	{
		CanTxType_Flag.bits.b1CanTx_0x07 = 0;
		CAN_TX_0x07();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x08)
	{
		CanTxType_Flag.bits.b1CanTx_0x08 = 0;
		CAN_TX_0x08();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x09)
	{
		CanTxType_Flag.bits.b1CanTx_0x09 = 0;
		CAN_TX_0x09();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x0A)
	{
		CanTxType_Flag.bits.b1CanTx_0x0A = 0;
		CAN_TX_0x0A();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x0B)
	{
		CanTxType_Flag.bits.b1CanTx_0x0B = 0;
		CAN_TX_0x0B();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x0C)
	{
		CanTxType_Flag.bits.b1CanTx_0x0C = 0;
		CAN_TX_0x0C();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x0D)
	{
		CanTxType_Flag.bits.b1CanTx_0x0D = 0;
		CAN_TX_0x0D();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x0E)
	{
		CanTxType_Flag.bits.b1CanTx_0x0E = 0;
		CAN_TX_0x0E();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x0F)
	{
		CanTxType_Flag.bits.b1CanTx_0x0F = 0;
		CAN_TX_0x0F();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x10)
	{
		CanTxType_Flag.bits.b1CanTx_0x10 = 0;
		CAN_TX_0x10();
	}
	else if (CanTxType_Flag.bits.b1CanTx_0x11)
	{
		CanTxType_Flag.bits.b1CanTx_0x11 = 0;
		CAN_TX_0x11();
	}
	else if (CanTxType_Flag.bits.b1CanTx_Test)
	{
		CanTxType_Flag.bits.b1CanTx_Test = 0;
		CAN_TX_Test();
	}
}

void InitCan(void)
{
#if 0
	Can_Status_Flag.all = 0;
	CanTxType_Flag.all = 0;
	InitCan_GPIO();
	InitCan_NVIC();
	InitCan_Filter();
	InitCan_CAN1();			//目前是回环模式，要改回普通模式,Test
#endif
	Can_Status_Flag.all = 0;
	CanTxType_Flag.all = 0;
	InitCan_GPIO();
	InitCan_NVIC();
	InitCan_CAN1();	  // 目前是回环模式，要改回普通模式,Test
	InitCan_Filter(); // 这个调到后面，RX也可以了
	feidao_can_power_off();
	feidao_can_invalidate_hw_tick();
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
	if ((s_u8AppTxPending != 0U) ||
		(s_u8AppTxMailbox != CAN_TxStatus_NoMailBox) ||
		(s_u8ReadBlockActive != 0U))
	{
		return 1U;
	}
	if ((CAN1->TSR & CAN_TSR_TME) != CAN_TSR_TME)
	{
		return 1U;
	}

	return 0U;
}

UINT8 Can_IsSleepBlocked(void)
{
	if (s_u8FeidaoCanPowerState != FEIDAO_CAN_POWER_IDLE)
	{
		return 1U;
	}
	if (s_u8FeidaoCanTxMailbox != CAN_TxStatus_NoMailBox)
	{
		return 1U;
	}
	if ((s_u8AppTxPending != 0U) ||
		(s_u8AppTxMailbox != CAN_TxStatus_NoMailBox) ||
		(s_u8ReadBlockActive != 0U))
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
	s_u8FeidaoCanProbeActive = 0U;
	s_u8FeidaoCanRtcServiceActive = 0U;
	s_u16FeidaoCanPendingMask = 0U;
	feidao_can_stop_read_block_stream();
	s_u8AppTxPending = 0U;
	s_u8AppCmdPending = 0U;
	if (s_u8AppTxMailbox != CAN_TxStatus_NoMailBox)
	{
		feidao_can_cancel_tx(s_u8AppTxMailbox);
		s_u8AppTxMailbox = CAN_TxStatus_NoMailBox;
	}
	feidao_can_abort_all_tx();
	s_u8FeidaoCanTxMailbox = CAN_TxStatus_NoMailBox;
	feidao_can_power_off();
	feidao_can_invalidate_hw_tick();
}

UINT8 Can_IsBusActive(void)
{
	return s_u8FeidaoCanBusActive;
}

UINT32 Can_GetIdleRtcPeriodSeconds(void)
{
	return s_u8FeidaoCanBusActive ? FEIDAO_CAN_RTC_ACTIVE_PERIOD_SECONDS : FEIDAO_CAN_RTC_IDLE_PERIOD_SECONDS;
}

void Can_RtcWakeService(UINT32 elapsed_seconds)
{
	s_u32FeidaoCanLogicalTick += feidao_can_seconds_to_ticks(elapsed_seconds);
	feidao_can_invalidate_hw_tick();

	InitCan();

	if (0U == s_u8FeidaoCanBusActive)
	{
		feidao_can_start_idle_probe();
		s_u8FeidaoCanRtcServiceActive = 1U;
		feidao_can_send(s_u32FeidaoCanLogicalTick);
		(void)feidao_can_service_until_idle(FEIDAO_CAN_RTC_SERVICE_TIMEOUT_TICKS);
		s_u8FeidaoCanRtcServiceActive = 0U;
		s_u8FeidaoCanProbeActive = 0U;

		if (0U == s_u8FeidaoCanBusActive)
		{
			s_u16FeidaoCanPendingMask = 0U;
		}
		else
		{
			feidao_can_anchor_schedule(s_u32FeidaoCanLogicalTick);
		}

		Can_PrepareSleep();
		return;
	}

	feidao_can_schedule_rtc_period_frames(s_u32FeidaoCanLogicalTick, elapsed_seconds);
	s_u8FeidaoCanRtcServiceActive = 1U;
	feidao_can_send(s_u32FeidaoCanLogicalTick);
	(void)feidao_can_service_until_idle(FEIDAO_CAN_RTC_SERVICE_TIMEOUT_TICKS);
	s_u8FeidaoCanRtcServiceActive = 0U;
	Can_PrepareSleep();
}

// 这个函数不能用Switch架构来解决，因为这个都是并行任务，不是串行。
void App_Can(void)
{
	UINT32 now_tick = feidao_can_update_logical_tick(SysTime_Get10msTickCount());

	if (g_st_SysTimeFlag.bits.b1Sys10msFlag)
	{
		Can_BusOFF_Monitor();
	}

	feidao_can_service_app_cmd();
	feidao_can_service_read_block_stream(now_tick);
	feidao_can_service_app_tx(now_tick);
	feidao_can_send(now_tick);
	feidao_can_service_app_tx(now_tick);
	feidao_can_service_enter_iap_delay();
}
void USB_LP_CAN1_RX0_IRQHandler(void)
{
	CanRxMsg rx_msg;

	while (CAN_MessagePending(CAN1, CAN_FIFO0) != 0U)
	{
		sys_time.can_rcv_cnt++;
		memset(&rx_msg, 0, sizeof(rx_msg));
		CAN_Receive(CAN1, CAN_FIFO0, &rx_msg);
		if (feidao_can_handle_rx_msg(&rx_msg) != 0U)
		{
			continue;
		}
		RxMessage = rx_msg;
		Can_Status_Flag.bits.b1Can_Received = 1;
	}
}
