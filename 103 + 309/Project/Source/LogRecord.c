#include "main.h"

#define EVENT_RECORD_LENGTH               100U
#define LOG_STORAGE_DELTA_MAGIC            ((UINT32)0x4C474432U) /* LGD2 */
#define LOG_STORAGE_SIGNATURE_OFFSET       ((UINT32)2166136261U)
#define LOG_STORAGE_SIGNATURE_PRIME        ((UINT32)16777619U)

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

typedef struct LOG_STORAGE_DELTA_RECORD_TAG
{
	UINT32 magic;
	UINT32 baseSignature;
	UINT8 event;
	UINT8 delta;
	UINT16 crc;
} LOG_STORAGE_DELTA_RECORD;

UINT32 su32_Interval_S_Tcnt = 0;

static LogRecordRuntime s_log_record;
static UINT32 s_log_base_signature;

static UINT16 LogStorage_Crc16(const UINT8 *data, UINT16 length)
{
	UINT16 crc = 0xFFFFU;
	UINT16 i;
	UINT8 bit;

	for (i = 0U; i < length; ++i)
	{
		crc ^= data[i];
		for (bit = 0U; bit < 8U; ++bit)
		{
			if ((crc & 1U) != 0U)
			{
				crc = (UINT16)((crc >> 1) ^ 0xA001U);
			}
			else
			{
				crc >>= 1;
			}
		}
	}

	return crc;
}

static UINT32 LogStorage_BaseSignature(UINT8 point,
										const UINT8 records[EVENT_RECORD_LENGTH][2])
{
	UINT32 hash = LOG_STORAGE_SIGNATURE_OFFSET;
	UINT16 i;

	hash ^= point;
	hash *= LOG_STORAGE_SIGNATURE_PRIME;
	for (i = 0U; i < EVENT_RECORD_LENGTH; ++i)
	{
		hash ^= records[i][0];
		hash *= LOG_STORAGE_SIGNATURE_PRIME;
		hash ^= records[i][1];
		hash *= LOG_STORAGE_SIGNATURE_PRIME;
	}

	return hash;
}

static UINT8 LogStorage_RecordIsBlank(UINT32 addr)
{
	UINT16 offset;

	for (offset = 0U; offset < (UINT16)sizeof(LOG_STORAGE_DELTA_RECORD); offset += 2U)
	{
		if (FlashReadOneHalfWord(addr + offset) != 0xFFFFU)
		{
			return 0U;
		}
	}
	return 1U;
}

static UINT8 LogStorage_ReadDelta(UINT32 addr, LOG_STORAGE_DELTA_RECORD *record)
{
	LOG_STORAGE_DELTA_RECORD temp;
	UINT16 crc;

	if (record == 0)
	{
		return 0U;
	}

	memcpy(&temp, (const void *)addr, sizeof(temp));
	if (temp.magic != LOG_STORAGE_DELTA_MAGIC)
	{
		return 0U;
	}

	crc = LogStorage_Crc16((const UINT8 *)&temp,
								(UINT16)(sizeof(temp) - sizeof(temp.crc)));
	if (crc != temp.crc)
	{
		return 0U;
	}
	if ((temp.event >= EVENT_NUM) || (temp.delta > 171U))
	{
		return 0U;
	}

	*record = temp;
	return 1U;
}

static UINT8 LogStorage_ProgramDelta(UINT32 addr,
										LogEventArray event,
										UINT8 delta)
{
	LOG_STORAGE_DELTA_RECORD record;
	LOG_STORAGE_DELTA_RECORD verify;
	const UINT8 *bytes;
	UINT16 offset;
	UINT16 halfWord;
	FLASH_Status status;

	if (!LogStorage_RecordIsBlank(addr))
	{
		return 0U;
	}

	record.magic = LOG_STORAGE_DELTA_MAGIC;
	record.baseSignature = s_log_base_signature;
	record.event = (UINT8)event;
	record.delta = delta;
	record.crc = LogStorage_Crc16((const UINT8 *)&record,
									 (UINT16)(sizeof(record) - sizeof(record.crc)));
	bytes = (const UINT8 *)&record;

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	for (offset = 0U; offset < (UINT16)sizeof(record); offset += 2U)
	{
		halfWord = (UINT16)bytes[offset] |
				   (UINT16)((UINT16)bytes[offset + 1U] << 8);
		status = FLASH_ProgramHalfWord(addr + offset, halfWord);
		if ((status != FLASH_COMPLETE) ||
			(FlashReadOneHalfWord(addr + offset) != halfWord))
		{
			FLASH_Lock();
			System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			return 0U;
		}
	}
	FLASH_Lock();

	if (!LogStorage_ReadDelta(addr, &verify) ||
		(verify.baseSignature != record.baseSignature) ||
		(verify.event != record.event) ||
		(verify.delta != record.delta))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}
	return 1U;
}

static UINT8 LogStorage_EraseDeltaPage(UINT32 pageAddr)
{
	UINT32 offset;
	FLASH_Status status;

	if (LogStorage_RecordIsBlank(pageAddr) &&
		LogStorage_RecordIsBlank(pageAddr + FLASH_STORAGE_PAGE_SIZE - sizeof(LOG_STORAGE_DELTA_RECORD)))
	{
		return 1U;
	}

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	status = FLASH_ErasePage(pageAddr);
	FLASH_Lock();
	if (status != FLASH_COMPLETE)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}

	for (offset = 0U; offset < FLASH_STORAGE_PAGE_SIZE; offset += 2U)
	{
		if (FlashReadOneHalfWord(pageAddr + offset) != 0xFFFFU)
		{
			System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			return 0U;
		}
	}
	return 1U;
}

static UINT32 LogStorage_FindBlankInPage(UINT32 pageAddr)
{
	UINT32 offset;

	for (offset = 0U;
		 (offset + sizeof(LOG_STORAGE_DELTA_RECORD)) <= FLASH_STORAGE_PAGE_SIZE;
		 offset += sizeof(LOG_STORAGE_DELTA_RECORD))
	{
		if (LogStorage_RecordIsBlank(pageAddr + offset))
		{
			return pageAddr + offset;
		}
	}
	return 0U;
}

static void LogStorage_ApplyEntry(UINT8 *point,
								 UINT8 records[EVENT_RECORD_LENGTH][2],
								 UINT8 event,
								 UINT8 delta)
{
	if (*point >= EVENT_RECORD_LENGTH)
	{
		*point = 0U;
	}

	records[*point][0] = event;
	records[*point][1] = delta;
	++(*point);
}

static UINT8 LogStorage_ReplayPage(UINT32 pageAddr,
									UINT8 *point,
									UINT8 records[EVENT_RECORD_LENGTH][2])
{
	UINT32 offset;
	LOG_STORAGE_DELTA_RECORD deltaRecord;

	for (offset = 0U;
		 (offset + sizeof(LOG_STORAGE_DELTA_RECORD)) <= FLASH_STORAGE_PAGE_SIZE;
		 offset += sizeof(LOG_STORAGE_DELTA_RECORD))
	{
		if (LogStorage_RecordIsBlank(pageAddr + offset))
		{
			break;
		}
		if (!LogStorage_ReadDelta(pageAddr + offset, &deltaRecord))
		{
			return 0U;
		}
		if (deltaRecord.baseSignature == s_log_base_signature)
		{
			LogStorage_ApplyEntry(point, records, deltaRecord.event, deltaRecord.delta);
		}
	}
	return 1U;
}

static UINT8 LogStorage_ReplayDeltas(UINT8 *point,
									  UINT8 records[EVENT_RECORD_LENGTH][2])
{
	if (!LogStorage_ReplayPage(FLASH_ADDR_STORAGE_LOG_DELTA_A, point, records))
	{
		return 0U;
	}
	if (!LogStorage_ReplayPage(FLASH_ADDR_STORAGE_LOG_DELTA_B, point, records))
	{
		return 0U;
	}
	return 1U;
}

static UINT8 LogStorage_CompactWithEvent(LogEventArray event, UINT8 delta)
{
	UINT8 point = s_log_record.point;
	UINT8 records[EVENT_RECORD_LENGTH][2];
	UINT8 eraseA;
	UINT8 eraseB;

	memcpy(records, s_log_record.records, sizeof(records));
	LogStorage_ApplyEntry(&point, records, (UINT8)event, delta);
	if (!StorageFlash_SaveLogData(point, (const UINT8(*)[2])records))
	{
		return 0U;
	}

	s_log_base_signature = LogStorage_BaseSignature(point, records);
	eraseA = LogStorage_EraseDeltaPage(FLASH_ADDR_STORAGE_LOG_DELTA_A);
	eraseB = LogStorage_EraseDeltaPage(FLASH_ADDR_STORAGE_LOG_DELTA_B);
	if (!eraseA || !eraseB)
	{
		/* The new base is already committed. Stale deltas have an old signature
		 * and will be ignored after reboot, so cleanup failure is non-fatal. */
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
	}
	return 1U;
}

static UINT8 LogStorage_PersistEvent(LogEventArray event, UINT8 delta)
{
	UINT32 addr;

	addr = LogStorage_FindBlankInPage(FLASH_ADDR_STORAGE_LOG_DELTA_A);
	if (addr == 0U)
	{
		addr = LogStorage_FindBlankInPage(FLASH_ADDR_STORAGE_LOG_DELTA_B);
	}
	if (addr != 0U)
	{
		return LogStorage_ProgramDelta(addr, event, delta);
	}

	return LogStorage_CompactWithEvent(event, delta);
}

static UINT8 LogRecord_CanSaveEvent(LogEventArray event)
{
#if PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC > 0
	UINT32 elapsed;

	if ((event == BMS_START_UP) || (event == BMS_SLEEP))
	{
		return 1U;
	}
	if (event >= EVENT_NUM)
	{
		return 0U;
	}
	if (!s_log_record.lastSaveValid[event])
	{
		return 1U;
	}

	elapsed = s_log_record.uptimeSeconds - s_log_record.lastSaveSeconds[event];
	return (elapsed >= (UINT32)PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC) ? 1U : 0U;
#else
	(void)event;
	return 1U;
#endif
}

static void LogRecord_MarkEventSaved(LogEventArray event)
{
	if (event < EVENT_NUM)
	{
		s_log_record.lastSaveSeconds[event] = s_log_record.uptimeSeconds;
		s_log_record.lastSaveValid[event] = 1U;
	}
}

static UINT8 LogRecord_IsEntryValid(UINT8 event, UINT8 delta)
{
	if (event >= EVENT_NUM)
	{
		return 0U;
	}
	if (delta > 171U)
	{
		return 0U;
	}
	return 1U;
}

static UINT8 LogTime_MapValue(UINT32 timeSeconds)
{
	if (timeSeconds <= 60U)
	{
		return 171U;
	}
	if (timeSeconds <= (3600UL * 168UL))
	{
		return (UINT8)(timeSeconds / 3600U +
						 (((timeSeconds % 3600U) > 0U) ? 1U : 0U));
	}
	return 170U;
}

void LogRecord_RequestStartup(void)
{
	s_log_record.flags.bits.Log_StartUp = 1U;
}

void LogRecord_RequestSleep(void)
{
	LogEvent_Record(1U, BMS_SLEEP, &su32_Interval_S_Tcnt);
}

UINT8 LogTime_Map(UINT32 *Time_S_Cnt)
{
	UINT8 result;

	result = LogTime_MapValue(*Time_S_Cnt);
	*Time_S_Cnt = 0U;
	return result;
}

static UINT8 LogEvent_EEPROM(LogEventArray event, UINT32 *Time_S_Cnt)
{
	UINT8 delta;

	if (!LogRecord_CanSaveEvent(event))
	{
		return 0U;
	}
	if (event >= EVENT_NUM)
	{
		return 0U;
	}

	delta = (event == BMS_START_UP) ? 0U : LogTime_MapValue(*Time_S_Cnt);
	if (!LogStorage_PersistEvent(event, delta))
	{
		return 0U;
	}

	LogStorage_ApplyEntry(&s_log_record.point, s_log_record.records, (UINT8)event, delta);
	*Time_S_Cnt = 0U;
	LogRecord_MarkEventSaved(event);
	return 1U;
}

void LogEvent_Record(UINT8 temp, LogEventArray event, UINT32 *Time_S_Cnt)
{
	if (BMS_START_UP == event)
	{
		if (s_log_record.flags.bits.Log_StartUp && LogEvent_EEPROM(event, Time_S_Cnt))
		{
			s_log_record.flags.bits.Log_StartUp = 0U;
		}
	}
	else if (BMS_SLEEP == event)
	{
		(void)LogEvent_EEPROM(event, Time_S_Cnt);
	}
	else if (CBC_ERR == event)
	{
		if ((s_log_record.cbcTemp != temp) && LogEvent_EEPROM(event, Time_S_Cnt))
		{
			s_log_record.cbcTemp = temp;
		}
	}
	else if (event < EVENT_NUM)
	{
		switch (s_log_record.eventLatch[event])
		{
		case 0:
			if (temp && LogEvent_EEPROM(event, Time_S_Cnt))
			{
				s_log_record.eventLatch[event] = 1U;
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
	temp = System_ERROR_UserCallback(ERROR_STATUS_EEPROM_STORE) +
		   System_ERROR_UserCallback(ERROR_STATUS_EEPROM_COM);
	LogEvent_Record(temp, EEPROM_ERR, &su32_Interval_S_Tcnt);
	LogEvent_Record(System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG), CBC_ERR, &su32_Interval_S_Tcnt);
}

void Sci_ACK_0x03_ReadRegs_EventRecord(UINT8 t_u8BuffTemp[])
{
	UINT16 i = 0U;
	UINT16 j;
	INT8 k;

	for (j = 0U; j < EVENT_RECORD_LENGTH; ++j)
	{
		k = (INT8)(s_log_record.point - 1 - j);
		if (k < 0)
		{
			k = (INT8)(EVENT_RECORD_LENGTH + k);
		}
		t_u8BuffTemp[i++] = s_log_record.records[(UINT8)k][0];
		t_u8BuffTemp[i++] = s_log_record.records[(UINT8)k][1];
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
	UINT8 emptyRecords[EVENT_RECORD_LENGTH][2] = {{0}};
	UINT8 eraseA;
	UINT8 eraseB;

	if (!StorageFlash_SaveLogData(0U, (const UINT8(*)[2])emptyRecords))
	{
		return 0U;
	}

	s_log_base_signature = LogStorage_BaseSignature(0U, emptyRecords);
	eraseA = LogStorage_EraseDeltaPage(FLASH_ADDR_STORAGE_LOG_DELTA_A);
	eraseB = LogStorage_EraseDeltaPage(FLASH_ADDR_STORAGE_LOG_DELTA_B);

	memset(s_log_record.records, 0, sizeof(s_log_record.records));
	s_log_record.point = 0U;

	if (!eraseA || !eraseB)
	{
		/* The new empty base is committed and stale deltas no longer match it,
		 * but a physical erase failure must remain visible to diagnostics. */
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}

	return 1U;
}

void ReadEEPROM_EventRecord_Parameters(void)
{
	UINT8 i;
	UINT8 point = 0U;
	UINT8 invalid = 0U;

	if (!StorageFlash_LoadLogData(&point, s_log_record.records))
	{
		invalid = 1U;
	}
	else if (point > EVENT_RECORD_LENGTH)
	{
		invalid = 1U;
	}
	else
	{
		for (i = 0U; i < EVENT_RECORD_LENGTH; ++i)
		{
			if (!LogRecord_IsEntryValid(s_log_record.records[i][0],
										 s_log_record.records[i][1]))
			{
				invalid = 1U;
				break;
			}
		}
	}

	if (invalid)
	{
		if (!EEPROM_ResetData_EventRecord_ToDefault())
		{
			System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		}
		return;
	}

	s_log_base_signature = LogStorage_BaseSignature(point, s_log_record.records);
	if (!LogStorage_ReplayDeltas(&point, s_log_record.records))
	{
		if (!EEPROM_ResetData_EventRecord_ToDefault())
		{
			System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		}
		return;
	}
	s_log_record.point = point;
}
