#include "main.h"

#define EVENT_RECORD_LENGTH 100

UINT8 BMS_LOG_POINT = 0;
UINT8 BMS_LOG_RECORD[EVENT_RECORD_LENGTH][2];
LOG_RECORD_FLAG LogRecord_Flag;
UINT8 gu8_Reset_EventRecord = 0;
UINT32 su32_Interval_S_Tcnt = 0;

static UINT8 LogRecord_IsEntryValid(UINT8 event, UINT8 delta)
{
	if (event > EVENT_NUM)
	{
		return 0;
	}

	if (delta > 171)
	{
		return 0;
	}

	return 1;
}

UINT8 LogTime_Map(UINT32 *Time_S_Cnt)
{
	UINT8 result = 0;

	if ((*Time_S_Cnt) <= 60)
	{
		result = 171;
	}
	else if ((*Time_S_Cnt) <= 3600 * 168)
	{
		result = (UINT8)((*Time_S_Cnt) / 3600 + (((*Time_S_Cnt) % 3600) > 0 ? 1 : 0));
	}
	else
	{
		result = 170;
	}

	(*Time_S_Cnt) = 0;
	return result;
}

void LogEvent_EEPROM(LogEventArray event, UINT32 *Time_S_Cnt)
{
	if (BMS_LOG_POINT >= EVENT_RECORD_LENGTH)
	{
		BMS_LOG_POINT = 0;
	}

	BMS_LOG_RECORD[BMS_LOG_POINT][0] = event;
	BMS_LOG_RECORD[BMS_LOG_POINT][1] = LogTime_Map(Time_S_Cnt);
	if (event == BMS_START_UP)
	{
		BMS_LOG_RECORD[BMS_LOG_POINT][1] = 0;
	}
	++BMS_LOG_POINT;

	StorageFlash_SaveLogData(BMS_LOG_POINT, BMS_LOG_RECORD);
}

void LogEvent_Record(UINT8 temp, LogEventArray event, UINT32 *Time_S_Cnt)
{
	static UINT8 su8_Event[EVENT_NUM] = {0};
	static UINT8 su8_CBC_Temp = 0;

	if (BMS_START_UP == event)
	{
		if (LogRecord_Flag.bits.Log_StartUp)
		{
			LogEvent_EEPROM(event, Time_S_Cnt);
			LogRecord_Flag.bits.Log_StartUp = 0;
		}
	}
	else if (BMS_SLEEP == event)
	{
		if (LogRecord_Flag.bits.Log_Sleep)
		{
			LogEvent_EEPROM(event, Time_S_Cnt);
			LogRecord_Flag.bits.Log_Sleep = 0;
			Sleep_Mode.bits.b1_ToSleepFlag = 0;
		}
	}
	else if (CBC_ERR == event)
	{
		if (su8_CBC_Temp != temp)
		{
			su8_CBC_Temp = temp;
			LogEvent_EEPROM(event, Time_S_Cnt);
		}
	}
	else
	{
		switch (su8_Event[event])
		{
		case 0:
			if (temp)
			{
				LogEvent_EEPROM(event, Time_S_Cnt);
				su8_Event[event] = 1;
			}
			break;

		case 1:
			if (!temp)
			{
				su8_Event[event] = 0;
			}
			break;

		default:
			break;
		}
	}
}

void App_LogRecord(void)
{
	UINT8 temp;

	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag3)
	{
		return;
	}

	++su32_Interval_S_Tcnt;

	LogEvent_Record(LogRecord_Flag.bits.Log_StartUp, BMS_START_UP, &su32_Interval_S_Tcnt);
	LogEvent_Record(SystemStatus.bits.b1Status_Heat, HEAT_OPEN, &su32_Interval_S_Tcnt);
	LogEvent_Record(SystemStatus.bits.b1Status_Cool, COOL_OPEN, &su32_Interval_S_Tcnt);

	LogEvent_Record(g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp, VCELL_OVP, &su32_Interval_S_Tcnt);
	LogEvent_Record(g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp, VBUS_OVP, &su32_Interval_S_Tcnt);
	LogEvent_Record(g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp, CHG_OCP, &su32_Interval_S_Tcnt);

	LogEvent_Record(g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp, VCELL_UVP, &su32_Interval_S_Tcnt);
	LogEvent_Record(g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp, VBUS_UVP, &su32_Interval_S_Tcnt);
	LogEvent_Record(g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp, DSG_OCP, &su32_Interval_S_Tcnt);

	LogEvent_Record(g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp, CHG_UTP, &su32_Interval_S_Tcnt);
	LogEvent_Record(g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp, DSG_UTP, &su32_Interval_S_Tcnt);
	LogEvent_Record(g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp, CHG_OTP, &su32_Interval_S_Tcnt);
	LogEvent_Record(g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp, DSG_OTP, &su32_Interval_S_Tcnt);
	LogEvent_Record(g_stCellInfoReport.unMdlFault_Third.bits.b1VcellDeltaBig, VDELTA_OP, &su32_Interval_S_Tcnt);

	LogEvent_Record(System_ERROR_UserCallback(ERROR_STATUS_AFE2), AFE2_ERR, &su32_Interval_S_Tcnt);
	temp = System_ERROR_UserCallback(ERROR_STATUS_EEPROM_STORE) + System_ERROR_UserCallback(ERROR_STATUS_EEPROM_COM);
	LogEvent_Record(temp, EEPROM_ERR, &su32_Interval_S_Tcnt);
	LogEvent_Record(System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG), CBC_ERR, &su32_Interval_S_Tcnt);
}

void Sci_ACK_0x03_ReadRegs_EventRecord(UINT8 t_u8BuffTemp[])
{
	UINT16 i = 0;
	UINT16 j;
	INT8 k;

	for (j = 0; j < EVENT_RECORD_LENGTH; j++)
	{
		k = (INT8)(BMS_LOG_POINT - 1 - j);
		if (k < 0)
		{
			k = EVENT_RECORD_LENGTH + k;
		}
		t_u8BuffTemp[i++] = BMS_LOG_RECORD[k][0];
		t_u8BuffTemp[i++] = BMS_LOG_RECORD[k][1];
	}
}

void Sci_WrReg_0x06_Reset_EventRecord(struct RS485MSG *s)
{
	UINT16 u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (0x0001 == u16SciRegData)
	{
		if (!EEPROM_ResetData_EventRecord_ToDefault())
		{
			s->AckType = RS485_ACK_NEG;
			s->ErrorType = RS485_ERROR_CMD_INVALID;
		}
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

UINT8 EEPROM_ResetData_EventRecord_ToDefault(void)
{
	UINT8 i;

	for (i = 0; i < EVENT_RECORD_LENGTH; ++i)
	{
		BMS_LOG_RECORD[i][0] = 0;
		BMS_LOG_RECORD[i][1] = 0;
	}
	BMS_LOG_POINT = 0;
	gu8_Reset_EventRecord = 0;

	return StorageFlash_SaveLogData(BMS_LOG_POINT, BMS_LOG_RECORD);
}

void ReadEEPROM_EventRecord_Parameters(void)
{
	UINT8 i;
	UINT8 point = 0;
	UINT8 invalid = 0;

	if (!StorageFlash_LoadLogData(&point, BMS_LOG_RECORD))
	{
		invalid = 1;
	}
	else if (point > EVENT_RECORD_LENGTH)
	{
		invalid = 1;
	}
	else
	{
		for (i = 0; i < EVENT_RECORD_LENGTH; ++i)
		{
			if (!LogRecord_IsEntryValid(BMS_LOG_RECORD[i][0], BMS_LOG_RECORD[i][1]))
			{
				invalid = 1;
				break;
			}
		}
	}

	if (invalid)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		EEPROM_ResetData_EventRecord_ToDefault();
		return;
	}

	BMS_LOG_POINT = point;
}
