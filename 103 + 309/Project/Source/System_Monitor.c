#include "main.h"

volatile struct SYSTEM_ERROR System_ErrFlag;
volatile union System_OnOFF_Function System_OnOFF_Func;
volatile union System_OnOFF_Function System_OnOFF_Func_StartUpRec;
volatile union System_Status SystemStatus;
volatile union System_Function_StartUp System_Func_StartUp;

#define SYSTEM_ERROR_FIELD_INVALID ((UINT8)0xFFU)
/* Keep these masks in sync with System_Monitor.h bitfield order. */
#define SYSTEM_ONOFF_DEFAULT_MASK ((UINT32)0x00000287U)
#define SYSTEM_STARTUP_DEFAULT_MASK ((UINT32)0x0000087FU)
#define SYSTEM_STATUS_DEFAULT_MASK ((UINT32)0x00000001U)

static const UINT8 s_u8SystemErrorFieldOffset[ERROR_NUM + 1] = {
	SYSTEM_ERROR_FIELD_INVALID,
	0U,  1U,  2U,  3U,
	4U,  5U,  6U,  7U,
	8U,  9U,  10U, 11U,
	20U, 12U, 13U, 14U,
	15U, 16U, 17U, 21U,
	SYSTEM_ERROR_FIELD_INVALID,
	SYSTEM_ERROR_FIELD_INVALID,
	22U
};

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
	System_OnOFF_Func.all = SYSTEM_ONOFF_DEFAULT_MASK;
	System_Func_StartUp.all = SYSTEM_STARTUP_DEFAULT_MASK;
	SystemStatus.all = SYSTEM_STATUS_DEFAULT_MASK;
	System_OnOFF_Func_StartUpRec.all = System_OnOFF_Func.all;
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
		baseError = (enum SYSTEM_ERROR_COMMAND)((UINT16)ERROR_AFE1 +
												((UINT16)errorCode - (UINT16)ERROR_REMOVE_AFE1));
		flag = System_ErrorField(baseError);
		if (flag != 0)
		{
			*flag = 0U;
		}
		return result;
	}

	if ((errorCode >= ERROR_STATUS_AFE1) && (errorCode <= ERROR_STATUS_TEMP_BREAK))
	{
		baseError = (enum SYSTEM_ERROR_COMMAND)((UINT16)ERROR_AFE1 +
												((UINT16)errorCode - (UINT16)ERROR_STATUS_AFE1));
		flag = System_ErrorField(baseError);
		if (flag != 0)
		{
			result = *flag;
		}
	}

	return result;
}
