#include "main.h"
#include "CanFeidaoFrames.h"
#include "DebugWatch.h"

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

typedef struct CAN_FEIDAO_FRAME_DISPATCH_TAG
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

#if DEBUG_WATCH_ENABLED
void CanFeidaoFrames_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
	watch->tables.can_feidao_dispatch = s_can_feidao_dispatch;
	watch->tables.can_feidao_dispatch_count =
		(uint16_t)(sizeof(s_can_feidao_dispatch) / sizeof(s_can_feidao_dispatch[0]));
}
#endif

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

	return Can_HDX_TransmitPeriodic(&tx_msg);
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
	data[2] = 100;
	aging_remaining_min = 100;
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
