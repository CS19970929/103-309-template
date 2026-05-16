#include "main.h"
#include "BmsModel.h"
#include "CanFeidaoFrames.h"

#define CAN_FEIDAO_EXT_ID_BASE ((UINT32)0x14F80200U)
#define CAN_FEIDAO_FRAME_LEN_8 ((uint8_t)8U)

static void CanFeidao_PutU16Be(uint8_t *data, uint8_t offset, uint16_t value);
static void CanFeidao_PutU32Be(uint8_t *data, uint8_t offset, uint32_t value);
static void CanFeidao_PutI32Be(uint8_t *data, uint8_t offset, int32_t value);
static UINT8 CanFeidao_SendFrame(uint8_t chd_index, const uint8_t *data, uint8_t length);
static UINT8 CanFeidao_SendVoltageCurrent1000ms(void);
static UINT8 CanFeidao_SendCap5000ms(void);
static UINT8 CanFeidao_SendSoc1000ms(void);
static UINT8 CanFeidao_SendSoh5000ms(void);
static UINT8 CanFeidao_SendVersion5000ms(void);
static UINT8 CanFeidao_SendStatus5000ms(void);
static UINT8 CanFeidao_SendFactoryTime5000ms(void);

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

static void CanFeidao_PutI32Be(uint8_t *data, uint8_t offset, int32_t value)
{
	CanFeidao_PutU32Be(data, offset, (uint32_t)value);
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
	uint16_t charge_a10 = BmsModel_GetChargeCurrentA10();
	uint16_t discharge_a10 = BmsModel_GetDischargeCurrentA10();
	uint32_t voltage = BmsModel_GetPackVoltageMv();

	if (discharge_a10 > 0U)
	{
		current = -((int32_t)discharge_a10 * 100);
	}
	else
	{
		current = (int32_t)charge_a10 * 100;
	}

	CanFeidao_PutU32Be(data, 0U, voltage);
	CanFeidao_PutI32Be(data, 4U, current);
	return CanFeidao_SendFrame(0U, data, CAN_FEIDAO_FRAME_LEN_8);
}

static UINT8 CanFeidao_SendCap5000ms(void)
{
	uint8_t data[8] = {0U};
	uint32_t real_cap = (uint32_t)BmsModel_GetCapacityNowAh100() * 10U * BmsModel_GetPackVoltage10mV() / 100U;
	uint32_t design_cap = (uint32_t)BmsModel_GetCapacityFactoryAh100() * 10U * (36U * BmsModel_GetSeriesCount()) / 10U;

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
	const struct stCell_Info *cell = BmsModel_CellInfoConst();
	uint8_t chg_status;
	uint8_t soc;
	int8_t temp;
	uint8_t bat_type;
	uint16_t time_chg = 100U;
	uint16_t res = 0U;

	if (cell->unMdlFault_Third.bits.b1CellOvp || cell->unMdlFault_Third.bits.b1BatOvp)
	{
		chg_status = 2U;
	}
	else if (BmsModel_GetChargeCurrentA10())
	{
		chg_status = 1U;
	}
	else
	{
		chg_status = 0U;
	}

	soc = BmsModel_GetSocPercent();
	temp = (int8_t)((int16_t)BmsModel_GetMaxTemperatureT10Offset40() / 10 - 40);

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
	uint8_t soh = BmsModel_GetSohPercent();
	uint16_t cycles = BmsModel_GetCycleTimes();

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
	const struct stCell_Info *cell = BmsModel_CellInfoConst();
	const volatile union System_Status *status = BmsModel_SystemStatusConst();
	uint8_t work_status = 0U;
	uint8_t exception_status = 0U;
	uint16_t cap_fac;
	uint16_t cap_now;
	uint16_t cap_design;

	work_status |= (UINT8)(status->bits.b1Status_MOS_DSG << 0);
	work_status |= (UINT8)(status->bits.b1Status_MOS_CHG << 1);

	if (BmsModel_GetChargeCurrentA10())
	{
		work_status |= (UINT8)(1U << 2);
		work_status |= (UINT8)(1U << 3);
	}
	if (BmsModel_GetDischargeCurrentA10())
	{
		work_status |= (UINT8)(1U << 4);
	}

	if (cell->unMdlFault_Third.bits.b1IdischgOcp)
	{
		exception_status = 0x02U;
	}
	if (cell->unMdlFault_Third.bits.b1CellChgUtp)
	{
		exception_status = 0x03U;
	}
	if (cell->unMdlFault_Third.bits.b1CellChgOtp)
	{
		exception_status = 0x04U;
	}
	if (cell->unMdlFault_Third.bits.b1CellDischgOtp)
	{
		exception_status = 0x05U;
	}
	if (cell->unMdlFault_Third.bits.b1CellUvp || cell->unMdlFault_Third.bits.b1BatUvp)
	{
		exception_status = 0x06U;
	}
	if (cell->unMdlFault_Third.bits.b1CellOvp || cell->unMdlFault_Third.bits.b1BatOvp)
	{
		exception_status = 0x07U;
	}
	if (cell->unMdlFault_Third.bits.b1IchgOcp)
	{
		exception_status = 0x08U;
	}
	if (cell->unMdlFault_Third.bits.b1CellDischgUtp)
	{
		exception_status = 0x09U;
	}
	if (System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG))
	{
		exception_status = 0x0CU;
	}
	if (cell->unMdlFault_Third.bits.b1VcellDeltaBig)
	{
		exception_status = 0x0DU;
	}

	cap_fac = BmsModel_GetCapacityFactoryAh100() * 10U;
	cap_now = BmsModel_GetCapacityNowAh100() * 10U;
	cap_design = BmsModel_GetCapacityFactoryAh100() * 10U;

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

	CanFeidao_PutU16Be(data, 0U, BmsModel_GetCapacityFactoryAh100() * 10U);
	data[5] = FD_YEAR;
	data[6] = FD_MONTH;
	data[7] = FD_DAY;

	return CanFeidao_SendFrame(8U, data, CAN_FEIDAO_FRAME_LEN_8);
}

UINT8 CanFeidao_SendNextPending(UINT16 *pending_mask)
{
	if (*pending_mask & CAN_FEIDAO_MSG_VOLTAGE_CURRENT_1000MS)
	{
		*pending_mask &= (UINT16)(~CAN_FEIDAO_MSG_VOLTAGE_CURRENT_1000MS);
		return CanFeidao_SendVoltageCurrent1000ms();
	}
	if (*pending_mask & CAN_FEIDAO_MSG_SOC_1000MS)
	{
		*pending_mask &= (UINT16)(~CAN_FEIDAO_MSG_SOC_1000MS);
		return CanFeidao_SendSoc1000ms();
	}
	if (*pending_mask & CAN_FEIDAO_MSG_CAP_5000MS)
	{
		*pending_mask &= (UINT16)(~CAN_FEIDAO_MSG_CAP_5000MS);
		return CanFeidao_SendCap5000ms();
	}
	if (*pending_mask & CAN_FEIDAO_MSG_SOH_5000MS)
	{
		*pending_mask &= (UINT16)(~CAN_FEIDAO_MSG_SOH_5000MS);
		return CanFeidao_SendSoh5000ms();
	}
	if (*pending_mask & CAN_FEIDAO_MSG_VERSION_5000MS)
	{
		*pending_mask &= (UINT16)(~CAN_FEIDAO_MSG_VERSION_5000MS);
		return CanFeidao_SendVersion5000ms();
	}
	if (*pending_mask & CAN_FEIDAO_MSG_STATUS_5000MS)
	{
		*pending_mask &= (UINT16)(~CAN_FEIDAO_MSG_STATUS_5000MS);
		return CanFeidao_SendStatus5000ms();
	}
	if (*pending_mask & CAN_FEIDAO_MSG_FACTORY_TIME_5000MS)
	{
		*pending_mask &= (UINT16)(~CAN_FEIDAO_MSG_FACTORY_TIME_5000MS);
		return CanFeidao_SendFactoryTime5000ms();
	}

	return CAN_TxStatus_NoMailBox;
}
