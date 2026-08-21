#include "main.h"
#include "DebugWatch.h"

#define EVENT_RECORD_LENGTH 100

typedef struct LOG_RECORD_RUNTIME_TAG
{
	UINT8 point;
	UINT8 records[EVENT_RECORD_LENGTH][2];
	LOG_RECORD_FLAG flags;
	UINT32 uptimeSeconds;
	UINT32 lastSaveSeconds[EVENT_NUM];
	UINT8 lastSaveValid[EVENT_NUM];
	UINT8 eventLatch[EVENT_NUM];
	UINT8 cbcTemp;
} LogRecordRuntime;

UINT32 su32_Interval_S_Tcnt = 0;

static LogRecordRuntime s_log_record;

#if DEBUG_WATCH_ENABLED
void LogRecord_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
	watch->runtime.log_record = &s_log_record;
	watch->app.log_interval_s_tcnt = &su32_Interval_S_Tcnt;
}
#endif

static UINT8 LogRecord_CanSaveEvent(LogEventArray event)
{
#if PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC > 0
	UINT32 elapsed;

	if ((event == BMS_START_UP) || (event == BMS_SLEEP))
	{
		return 1;
	}

	if (event >= EVENT_NUM)
	{
		return 0;
	}

	if (!s_log_record.lastSaveValid[event])
	{
		return 1;
	}

	elapsed = s_log_record.uptimeSeconds - s_log_record.lastSaveSeconds[event];
	return (elapsed >= (UINT32)PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC) ? 1U : 0U;
#else
	(void)event;
	return 1;
#endif
}

static void LogRecord_MarkEventSaved(LogEventArray event)
{
	if (event < EVENT_NUM)
	{
		s_log_record.lastSaveSeconds[event] = s_log_record.uptimeSeconds;
		s_log_record.lastSaveValid[event] = 1;
	}
}

static UINT8 LogRecord_IsEntryValid(UINT8 event, UINT8 delta)
{
	if (event >= EVENT_NUM)
	{
		return 0;
	}

	if (delta > 171)
	{
		return 0;
	}

	return 1;
}

void LogRecord_RequestStartup(void)
{
	s_log_record.flags.bits.Log_StartUp = 1U;
}

void LogRecord_RequestSleep(void)
{
	LogEvent_Record(1U, BMS_SLEEP, &su32_Interval_S_Tcnt);
}

static UINT8 LogTime_Encode(UINT32 time_s)
{
	if (time_s <= 60U)
	{
		return 171U;
	}
	if (time_s <= (3600U * 168U))
	{
		return (UINT8)(time_s / 3600U + ((time_s % 3600U) > 0U ? 1U : 0U));
	}
	return 170U;
}

UINT8 LogTime_Map(UINT32 *Time_S_Cnt)
{
	UINT8 result;

	result = LogTime_Encode(*Time_S_Cnt);
	*Time_S_Cnt = 0U;
	return result;
}

/*
 * Commit order is intentional:
 *   1. persist the small delta record;
 *   2. update the RAM ring;
 *   3. update event latch/timestamp in the caller.
 *
 * A Flash failure therefore leaves the event retryable instead of making RAM
 * look newer than persistent storage.
 */
static UINT8 LogEvent_EEPROM(LogEventArray event, UINT32 *Time_S_Cnt)
{
	UINT8 delta;
	UINT8 point;

	if (!LogRecord_CanSaveEvent(event))
	{
		return 1U;
	}
	if ((event >= EVENT_NUM) || (Time_S_Cnt == 0))
	{
		return 0U;
	}

	point = s_log_record.point;
	if (point >= EVENT_RECORD_LENGTH)
	{
		point = 0U;
	}

	delta = (event == BMS_START_UP) ? 0U : LogTime_Encode(*Time_S_Cnt);
	if (Storage_LogAppend((UINT8)event, delta) == 0U)
	{
		return 0U;
	}

	s_log_record.records[point][0] = (UINT8)event;
	s_log_record.records[point][1] = delta;
	s_log_record.point = (UINT8)(point + 1U);
	*Time_S_Cnt = 0U;
	LogRecord_MarkEventSaved(event);
	return 1U;
}

void LogEvent_Record(UINT8 temp, LogEventArray event, UINT32 *Time_S_Cnt)
{
	if (event >= EVENT_NUM)
	{
		return;
	}

	if (BMS_START_UP == event)
	{
		if (s_log_record.flags.bits.Log_StartUp)
		{
			if (LogEvent_EEPROM(event, Time_S_Cnt) != 0U)
			{
				s_log_record.flags.bits.Log_StartUp = 0U;
			}
		}
	}
	else if (BMS_SLEEP == event)
	{
		(void)LogEvent_EEPROM(event, Time_S_Cnt);
	}
	else if (CBC_ERR == event)
	{
		if (s_log_record.cbcTemp != temp)
		{
			if (LogEvent_EEPROM(event, Time_S_Cnt) != 0U)
			{
				s_log_record.cbcTemp = temp;
			}
		}
	}
	else
	{
		switch (s_log_record.eventLatch[event])
		{
		case 0:
			if (temp)
			{
				if (LogEvent_EEPROM(event, Time_S_Cnt) != 0U)
				{
					s_log_record.eventLatch[event] = 1U;
				}
			}
			break;

		case 1:
			if (!temp)
			{
				s_log_record.eventLatch[event] = 0U;
			}
			break;

		default:
			s_log_record.eventLatch[event] = 0U;
			break;
		}
	}
}

void App_LogRecord(void)
{
	UINT8 temp;

	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag)
	{
		return;
	}

	++su32_Interval_S_Tcnt;
	++s_log_record.uptimeSeconds;

	LogEvent_Record(s_log_record.flags.bits.Log_StartUp, BMS_START_UP, &su32_Interval_S_Tcnt);

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
	INT16 k;

	for (j = 0; j < EVENT_RECORD_LENGTH; j++)
	{
		k = (INT16)s_log_record.point - 1 - (INT16)j;
		if (k < 0)
		{
			k = (INT16)(EVENT_RECORD_LENGTH + k);
		}
		t_u8BuffTemp[i++] = s_log_record.records[k][0];
		t_u8BuffTemp[i++] = s_log_record.records[k][1];
	}
}

void Sci_WrReg_0x06_Reset_EventRecord(struct RS485MSG *s)
{
	UINT16 u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);

	if (0x0000 == u16SciRegData)
	{
		return;
	}

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
	if (Storage_LogClear() == 0U)
	{
		return 0U;
	}

	memset(s_log_record.records, 0, sizeof(s_log_record.records));
	s_log_record.point = 0U;
	return 1U;
}

void ReadEEPROM_EventRecord_Parameters(void)
{
	UINT8 i;
	UINT8 point = 0;
	UINT8 invalid = 0;

	if (!Storage_LogLoad(&point, s_log_record.records))
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
			if (!LogRecord_IsEntryValid(s_log_record.records[i][0], s_log_record.records[i][1]))
			{
				invalid = 1;
				break;
			}
		}
	}

	if (invalid)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		memset(s_log_record.records, 0, sizeof(s_log_record.records));
		s_log_record.point = 0U;
		(void)Storage_LogClear();
		return;
	}

	s_log_record.point = point;
}
