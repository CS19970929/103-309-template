#include "main.h"
#include "DebugWatch.h"

volatile struct SYSTEM_ERROR System_ErrFlag;
static volatile union System_OnOFF_Function s_system_onoff_func;
static volatile union System_Status s_system_status;

#if DEBUG_WATCH_ENABLED
void SystemMonitor_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
	watch->system_feature = &s_system_onoff_func;
	watch->system_status = &s_system_status;
}
#endif

#define SYSTEM_ERROR_FIELD_INVALID ((UINT8)0xFFU)
/* Keep these masks in sync with System_Monitor.h bitfield order. */
#define SYSTEM_ONOFF_DEFAULT_MASK ((UINT32)0x00000287U)
#define SYSTEM_STATUS_DEFAULT_MASK ((UINT32)0x00000001U)

static const UINT8 s_u8SystemErrorFieldOffset[ERROR_NUM + 1] = {
	SYSTEM_ERROR_FIELD_INVALID,
	0U,  1U,  3U,  4U,
	5U,  6U,  7U,  8U,
	9U,  10U, 11U, 20U,
	12U, 13U, 14U, 15U,
	16U, 17U, 21U, 18U,
	19U, 22U
};

static enum SYSTEM_ERROR_COMMAND System_ErrorControlBase(enum SYSTEM_ERROR_COMMAND errorCode)
{
	switch (errorCode)
	{
	case ERROR_REMOVE_AFE1:
	case ERROR_STATUS_AFE1:
		return ERROR_AFE1;
	case ERROR_REMOVE_AFE2:
	case ERROR_STATUS_AFE2:
		return ERROR_AFE2;
	case ERROR_REMOVE_EEPROM_COM:
	case ERROR_STATUS_EEPROM_COM:
		return ERROR_EEPROM_COM;
	case ERROR_REMOVE_SPI:
	case ERROR_STATUS_SPI:
		return ERROR_SPI;
	case ERROR_REMOVE_UPPER:
	case ERROR_STATUS_UPPER:
		return ERROR_UPPER;
	case ERROR_REMOVE_CLIENT:
	case ERROR_STATUS_CLIENT:
		return ERROR_CLIENT;
	case ERROR_REMOVE_SCREEN:
	case ERROR_STATUS_SCREEN:
		return ERROR_SCREEN;
	case ERROR_REMOVE_WIFI:
	case ERROR_STATUS_WIFI:
		return ERROR_WIFI;
	case ERROR_REMOVE_BLUETOOTH:
	case ERROR_STATUS_BLUETOOTH:
		return ERROR_BLUETOOTH;
	case ERROR_REMOVE_APP:
	case ERROR_STATUS_APP:
		return ERROR_APP;
	case ERROR_REMOVE_CBC_CHG:
	case ERROR_STATUS_CBC_CHG:
		return ERROR_CBC_CHG;
	case ERROR_REMOVE_CBC_DSG:
	case ERROR_STATUS_CBC_DSG:
		return ERROR_CBC_DSG;
	case ERROR_REMOVE_EEPROM_STORE:
	case ERROR_STATUS_EEPROM_STORE:
		return ERROR_EEPROM_STORE;
	case ERROR_REMOVE_HSE:
	case ERROR_STATUS_HSE:
		return ERROR_HSE;
	case ERROR_REMOVE_LSE:
	case ERROR_STATUS_LSE:
		return ERROR_LSE;
	case ERROR_REMOVE_VDEATLE_OVER:
	case ERROR_STATUS_VDEATLE_OVER:
		return ERROR_VDEATLE_OVER;
	case ERROR_REMOVE_BALANCED:
	case ERROR_STATUS_BALANCED:
		return ERROR_BALANCED;
	case ERROR_REMOVE_ADC:
	case ERROR_STATUS_ADC:
		return ERROR_ADC;
	case ERROR_REMOVE_SOC_CAIL:
	case ERROR_STATUS_SOC_CAIL:
		return ERROR_SOC_CAIL;
	case ERROR_REMOVE_RESERVED_21:
	case ERROR_STATUS_RESERVED_21:
		return ERROR_RESERVED_21;
	case ERROR_REMOVE_RESERVED_22:
	case ERROR_STATUS_RESERVED_22:
		return ERROR_RESERVED_22;
	case ERROR_REMOVE_TEMP_BREAK:
	case ERROR_STATUS_TEMP_BREAK:
		return ERROR_TEMP_BREAK;
	default:
		return (enum SYSTEM_ERROR_COMMAND)0;
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
	s_system_onoff_func.all = SYSTEM_ONOFF_DEFAULT_MASK;
	s_system_status.all = SYSTEM_STATUS_DEFAULT_MASK;
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
	s_system_status.bits.b1Status_MOS_CHG = (charge_on != 0U) ? 1U : 0U;
	s_system_status.bits.b1Status_MOS_DSG = (discharge_on != 0U) ? 1U : 0U;
}

UINT8 SystemRuntime_IsChargeMosOpen(void)
{
	return s_system_status.bits.b1Status_MOS_CHG;
}

UINT8 SystemRuntime_IsDischargeMosOpen(void)
{
	return s_system_status.bits.b1Status_MOS_DSG;
}

UINT32 SystemRuntime_GetStatusSnapshot(void)
{
	return s_system_status.all;
}

UINT32 SystemFeature_GetMask(void)
{
	return s_system_onoff_func.all;
}

void SystemFeature_SetById(UINT16 function_id, UINT8 enable)
{
	UINT32 mask;

	if ((function_id == 0U) || (function_id > 32U))
	{
		return;
	}

	mask = ((UINT32)1U << (function_id - 1U));
	if (enable != 0U)
	{
		s_system_onoff_func.all |= mask;
	}
	else
	{
		s_system_onoff_func.all &= ~mask;
	}
}

UINT8 SystemFeature_IsSocFixed(void)
{
	return s_system_onoff_func.bits.b1OnOFF_SOC_Fixed;
}

UINT8 SystemFeature_IsSocZero(void)
{
	return s_system_onoff_func.bits.b1OnOFF_SOC_Zero;
}

UINT8 System_ERROR_UserCallback(enum SYSTEM_ERROR_COMMAND errorCode)
{
	UINT8 result = 0;
	volatile UINT8 *flag;
	enum SYSTEM_ERROR_COMMAND baseError;

	if ((errorCode >= ERROR_AFE1) && (errorCode <= ERROR_NUM))
	{
		flag = System_ErrorField(errorCode);
		if (flag != 0)
		{
			if (errorCode == ERROR_TEMP_BREAK)
			{
				*flag = 1U;
			}
			else if (errorCode != ERROR_EEPROM_STORE)
			{
				++(*flag);
			}
		}
		return result;
	}

	if ((errorCode >= ERROR_REMOVE_AFE1) && (errorCode <= ERROR_REMOVE_TEMP_BREAK))
	{
		baseError = System_ErrorControlBase(errorCode);
		flag = System_ErrorField(baseError);
		if (flag != 0)
		{
			*flag = 0U;
		}
		return result;
	}

	if ((errorCode >= ERROR_STATUS_AFE1) && (errorCode <= ERROR_STATUS_TEMP_BREAK))
	{
		baseError = System_ErrorControlBase(errorCode);
		flag = System_ErrorField(baseError);
		if (flag != 0)
		{
			result = *flag;
		}
	}

	return result;
}
