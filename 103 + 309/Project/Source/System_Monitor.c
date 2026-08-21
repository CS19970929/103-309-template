#include "main.h"

volatile struct SYSTEM_ERROR System_ErrFlag;
volatile union System_Status s_system_status;

#define SYSTEM_ERROR_FIELD_INVALID ((UINT8)0xFFU)
#define SYSTEM_STATUS_DEFAULT_MASK ((UINT32)0x00000001U)

static volatile struct BMS_CORE_STATE s_bms_core;
static volatile UINT32 s_bms_events;

static const UINT8 s_u8SystemErrorFieldOffset[ERROR_NUM + 1] = {
	SYSTEM_ERROR_FIELD_INVALID,
	0U,  1U,  3U,  4U,
	5U,  6U,  7U,  8U,
	9U,  10U, 11U, 20U,
	12U, 13U, 14U, 15U,
	16U, 17U, 21U, 18U,
	19U, 22U
};

static volatile UINT8 *System_ErrorField(enum SYSTEM_ERROR_COMMAND errorCode);

static UINT32 BmsCore_EnterCritical(void)
{
	UINT32 primask = __get_PRIMASK();
	__disable_irq();
	return primask;
}

static void BmsCore_ExitCritical(UINT32 primask)
{
	if ((primask & 1U) == 0U)
	{
		__enable_irq();
	}
}

void BmsEvent_Set(UINT32 events)
{
	UINT32 primask;

	if (events == 0U)
	{
		return;
	}

	primask = BmsCore_EnterCritical();
	s_bms_events |= events;
	BmsCore_ExitCritical(primask);
}

UINT32 BmsEvent_Take(UINT32 mask)
{
	UINT32 primask;
	UINT32 events;

	primask = BmsCore_EnterCritical();
	events = s_bms_events & mask;
	s_bms_events &= ~mask;
	BmsCore_ExitCritical(primask);
	return events;
}

UINT32 BmsEvent_Peek(void)
{
	UINT32 primask;
	UINT32 events;

	primask = BmsCore_EnterCritical();
	events = s_bms_events;
	BmsCore_ExitCritical(primask);
	return events;
}

void BmsCore_Init(void)
{
	UINT32 primask;

	primask = BmsCore_EnterCritical();
	memset((void *)&s_bms_core, 0, sizeof(s_bms_core));
	s_bms_events = 0U;
	BmsCore_ExitCritical(primask);
}

void BmsCore_UpdateMeasurement(UINT16 cell_min_mv,
							   UINT16 cell_max_mv,
							   UINT16 charge_current_a10,
							   UINT16 discharge_current_a10,
							   UINT16 soc_percent)
{
	s_bms_core.measurement.cellMinMv = cell_min_mv;
	s_bms_core.measurement.cellMaxMv = cell_max_mv;
	s_bms_core.measurement.chargeCurrentA10 = charge_current_a10;
	s_bms_core.measurement.dischargeCurrentA10 = discharge_current_a10;
	s_bms_core.measurement.socPercent = soc_percent;
}

void BmsCore_SetProtectionBlocks(UINT32 sw_charge_block,
								UINT32 sw_discharge_block,
								UINT32 hw_charge_block,
								UINT32 hw_discharge_block)
{
	if ((s_bms_core.protection.swChargeBlock != sw_charge_block) ||
		(s_bms_core.protection.swDischargeBlock != sw_discharge_block) ||
		(s_bms_core.protection.hwChargeBlock != hw_charge_block) ||
		(s_bms_core.protection.hwDischargeBlock != hw_discharge_block))
	{
		s_bms_core.protection.swChargeBlock = sw_charge_block;
		s_bms_core.protection.swDischargeBlock = sw_discharge_block;
		s_bms_core.protection.hwChargeBlock = hw_charge_block;
		s_bms_core.protection.hwDischargeBlock = hw_discharge_block;
		BmsEvent_Set(BMS_CORE_EVT_FAULT_CHANGED);
	}
}

UINT8 BmsCore_SetMosRequest(UINT8 charge_on, UINT8 discharge_on)
{
	UINT8 changed = 0U;

	charge_on = (charge_on != 0U) ? 1U : 0U;
	discharge_on = (discharge_on != 0U) ? 1U : 0U;

	if ((s_bms_core.mos.chargeRequest != charge_on) ||
		(s_bms_core.mos.dischargeRequest != discharge_on))
	{
		s_bms_core.mos.chargeRequest = charge_on;
		s_bms_core.mos.dischargeRequest = discharge_on;
		changed = 1U;
		BmsEvent_Set(BMS_CORE_EVT_MOS_REQUEST_CHANGED);
	}

	return changed;
}

void BmsCore_SetSleepBlockReason(UINT32 reason)
{
	if (s_bms_core.sleepBlockReason != reason)
	{
		s_bms_core.sleepBlockReason = reason;
		BmsEvent_Set(BMS_CORE_EVT_SLEEP_BLOCK_CHANGED);
	}
}

UINT32 BmsCore_GetSleepBlockReason(void)
{
	return s_bms_core.sleepBlockReason;
}

void BmsCore_GetState(struct BMS_CORE_STATE *state)
{
	UINT32 primask;

	if (state == 0)
	{
		return;
	}

	primask = BmsCore_EnterCritical();
	state->measurement.cellMinMv = s_bms_core.measurement.cellMinMv;
	state->measurement.cellMaxMv = s_bms_core.measurement.cellMaxMv;
	state->measurement.chargeCurrentA10 = s_bms_core.measurement.chargeCurrentA10;
	state->measurement.dischargeCurrentA10 = s_bms_core.measurement.dischargeCurrentA10;
	state->measurement.socPercent = s_bms_core.measurement.socPercent;
	state->protection.swChargeBlock = s_bms_core.protection.swChargeBlock;
	state->protection.swDischargeBlock = s_bms_core.protection.swDischargeBlock;
	state->protection.hwChargeBlock = s_bms_core.protection.hwChargeBlock;
	state->protection.hwDischargeBlock = s_bms_core.protection.hwDischargeBlock;
	state->mos.chargeRequest = s_bms_core.mos.chargeRequest;
	state->mos.dischargeRequest = s_bms_core.mos.dischargeRequest;
	state->mos.chargeActual = s_bms_core.mos.chargeActual;
	state->mos.dischargeActual = s_bms_core.mos.dischargeActual;
	state->sleepBlockReason = s_bms_core.sleepBlockReason;
	BmsCore_ExitCritical(primask);
}

static volatile UINT8 *System_ErrorCommandField(enum SYSTEM_ERROR_COMMAND errorCode)
{
	switch (errorCode)
	{
	case ERROR_REMOVE_AFE1:
	case ERROR_STATUS_AFE1:
		return System_ErrorField(ERROR_AFE1);
	case ERROR_REMOVE_AFE2:
	case ERROR_STATUS_AFE2:
		return System_ErrorField(ERROR_AFE2);
	case ERROR_REMOVE_CAN:
	case ERROR_STATUS_CAN:
		return &System_ErrFlag.u8ErrFlag_Com_Can;
	case ERROR_REMOVE_EEPROM_COM:
	case ERROR_STATUS_EEPROM_COM:
		return System_ErrorField(ERROR_EEPROM_COM);
	case ERROR_REMOVE_SPI:
	case ERROR_STATUS_SPI:
		return System_ErrorField(ERROR_SPI);
	case ERROR_REMOVE_UPPER:
	case ERROR_STATUS_UPPER:
		return System_ErrorField(ERROR_UPPER);
	case ERROR_REMOVE_CLIENT:
	case ERROR_STATUS_CLIENT:
		return System_ErrorField(ERROR_CLIENT);
	case ERROR_REMOVE_SCREEN:
	case ERROR_STATUS_SCREEN:
		return System_ErrorField(ERROR_SCREEN);
	case ERROR_REMOVE_WIFI:
	case ERROR_STATUS_WIFI:
		return System_ErrorField(ERROR_WIFI);
	case ERROR_REMOVE_BLUETOOTH:
	case ERROR_STATUS_BLUETOOTH:
		return System_ErrorField(ERROR_BLUETOOTH);
	case ERROR_REMOVE_APP:
	case ERROR_STATUS_APP:
		return System_ErrorField(ERROR_APP);
	case ERROR_REMOVE_CBC_CHG:
	case ERROR_STATUS_CBC_CHG:
		return System_ErrorField(ERROR_CBC_CHG);
	case ERROR_REMOVE_CBC_DSG:
	case ERROR_STATUS_CBC_DSG:
		return System_ErrorField(ERROR_CBC_DSG);
	case ERROR_REMOVE_EEPROM_STORE:
	case ERROR_STATUS_EEPROM_STORE:
		return System_ErrorField(ERROR_EEPROM_STORE);
	case ERROR_REMOVE_HSE:
	case ERROR_STATUS_HSE:
		return System_ErrorField(ERROR_HSE);
	case ERROR_REMOVE_LSE:
	case ERROR_STATUS_LSE:
		return System_ErrorField(ERROR_LSE);
	case ERROR_REMOVE_VDEATLE_OVER:
	case ERROR_STATUS_VDEATLE_OVER:
		return System_ErrorField(ERROR_VDEATLE_OVER);
	case ERROR_REMOVE_BALANCED:
	case ERROR_STATUS_BALANCED:
		return System_ErrorField(ERROR_BALANCED);
	case ERROR_REMOVE_ADC:
	case ERROR_STATUS_ADC:
		return System_ErrorField(ERROR_ADC);
	case ERROR_REMOVE_SOC_CAIL:
	case ERROR_STATUS_SOC_CAIL:
		return System_ErrorField(ERROR_SOC_CAIL);
	case ERROR_REMOVE_RESERVED_21:
	case ERROR_STATUS_RESERVED_21:
		return System_ErrorField(ERROR_RESERVED_21);
	case ERROR_REMOVE_RESERVED_22:
	case ERROR_STATUS_RESERVED_22:
		return System_ErrorField(ERROR_RESERVED_22);
	case ERROR_REMOVE_TEMP_BREAK:
	case ERROR_STATUS_TEMP_BREAK:
		return System_ErrorField(ERROR_TEMP_BREAK);
	default:
		return 0;
	}
}

static volatile UINT8 *System_ErrorField(enum SYSTEM_ERROR_COMMAND errorCode)
{
	UINT8 offset;

	if ((errorCode < ERROR_AFE1) || (errorCode > ERROR_NUM))
	{
		return 0;
	}

	offset = s_u8SystemErrorFieldOffset[(UINT8)errorCode];
	if (offset == SYSTEM_ERROR_FIELD_INVALID)
	{
		return 0;
	}

	return &(((volatile UINT8 *)&System_ErrFlag)[offset]);
}

void InitSystemMonitorData_EEPROM(void)
{
	s_system_status.all = SYSTEM_STATUS_DEFAULT_MASK;
	BmsCore_Init();
}

void SystemRuntime_MarkBootReady(void)
{
	s_system_status.bits.b1StartUpBMS = 0U;
	s_system_status.bits.b1Status_ToSleep = 1U;
}

void SystemRuntime_SetProjectVersion(UINT8 project_version)
{
	s_system_status.bits.b4Status_ProjectVer = (UINT8)(project_version & 0x0FU);
}

void SystemRuntime_SetAfeStatus(UINT8 afe_index, UINT8 is_ok)
{
	is_ok = (is_ok != 0U) ? 1U : 0U;
	if (afe_index == 0U)
	{
		s_system_status.bits.b1Status_AFE1 = is_ok;
	}
	else if (afe_index == 1U)
	{
		s_system_status.bits.b1Status_AFE2 = is_ok;
	}
}

void SystemRuntime_SetMosStatus(UINT8 charge_on, UINT8 discharge_on)
{
	UINT8 charge_actual = (charge_on != 0U) ? 1U : 0U;
	UINT8 discharge_actual = (discharge_on != 0U) ? 1U : 0U;

	if ((s_bms_core.mos.chargeActual != charge_actual) ||
		(s_bms_core.mos.dischargeActual != discharge_actual))
	{
		s_bms_core.mos.chargeActual = charge_actual;
		s_bms_core.mos.dischargeActual = discharge_actual;
		BmsEvent_Set(BMS_CORE_EVT_MOS_ACTUAL_CHANGED);
	}

	s_system_status.bits.b1Status_MOS_CHG = charge_actual;
	s_system_status.bits.b1Status_MOS_DSG = discharge_actual;
}

UINT8 SystemRuntime_IsChargeMosOpen(void)
{
	return s_bms_core.mos.chargeActual;
}

UINT8 SystemRuntime_IsDischargeMosOpen(void)
{
	return s_bms_core.mos.dischargeActual;
}

UINT32 SystemRuntime_GetStatusSnapshot(void)
{
	return s_system_status.all;
}

UINT8 System_ERROR_UserCallback(enum SYSTEM_ERROR_COMMAND errorCode)
{
	UINT8 result = 0;
	volatile UINT8 *flag;

	if ((errorCode >= ERROR_AFE1) && (errorCode <= ERROR_NUM))
	{
		flag = System_ErrorField(errorCode);
		if (flag != 0)
		{
			if (errorCode == ERROR_TEMP_BREAK)
			{
				*flag = 1U;
			}
			else if ((errorCode != ERROR_EEPROM_STORE) && (*flag < 0xFFU))
			{
				++(*flag);
			}
		}
		return result;
	}

	if ((errorCode >= ERROR_REMOVE_AFE1) && (errorCode <= ERROR_REMOVE_TEMP_BREAK))
	{
		flag = System_ErrorCommandField(errorCode);
		if (flag != 0)
		{
			*flag = 0U;
		}
		return result;
	}

	if ((errorCode >= ERROR_STATUS_AFE1) && (errorCode <= ERROR_STATUS_TEMP_BREAK))
	{
		flag = System_ErrorCommandField(errorCode);
		if (flag != 0)
		{
			result = *flag;
		}
	}

	return result;
}
