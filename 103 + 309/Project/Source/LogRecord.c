#include "main.h"

#define EVENT_RECORD_LENGTH                  FLASH_STORAGE_LOG_RECORD_COUNT
#define LOG_STORAGE_PAGE_MAGIC               ((UINT32)0x4C4F4733U) /* LOG3 */
#define LOG_STORAGE_PAGE_FLAG_RESET          ((UINT16)0x0001U)
#define LOG_STORAGE_PAGE_FLAGS_VALID         LOG_STORAGE_PAGE_FLAG_RESET

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
	UINT32 storagePage;
	UINT32 storageNextAddr;
	UINT32 storageGeneration;
} LogRecordRuntime;

typedef struct LOG_STORAGE_PAGE_HEADER_TAG
{
	UINT32 magic;
	UINT32 generation;
	UINT16 flags;
	UINT16 crc;
} LOG_STORAGE_PAGE_HEADER;

typedef struct LOG_STORAGE_ENTRY_TAG
{
	UINT8 event;
	UINT8 delta;
	UINT16 crc;
} LOG_STORAGE_ENTRY;

typedef struct LOG_STORAGE_PAGE_INFO_TAG
{
	UINT8 valid;
	UINT16 flags;
	UINT32 page;
	UINT32 generation;
	UINT32 nextAddr;
} LOG_STORAGE_PAGE_INFO;

typedef char LogStoragePageCapacityCheck[
	(((FLASH_STORAGE_PAGE_SIZE - sizeof(LOG_STORAGE_PAGE_HEADER)) /
	  sizeof(LOG_STORAGE_ENTRY)) >= EVENT_RECORD_LENGTH) ? 1 : -1];

UINT32 su32_Interval_S_Tcnt = 0;

static LogRecordRuntime s_log_record;

void LogEvent_Record(UINT8 temp, LogEventArray event, UINT32 *Time_S_Cnt);

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

static UINT8 LogStorage_IsBlank(UINT32 addr, UINT16 length)
{
	UINT16 offset;

	for (offset = 0U; offset < length; offset += 2U)
	{
		if (FlashReadOneHalfWord(addr + offset) != 0xFFFFU)
		{
			return 0U;
		}
	}
	return 1U;
}

static UINT16 LogStorage_HeaderCrc(const LOG_STORAGE_PAGE_HEADER *header)
{
	return StorageFlash_Crc16((const UINT8 *)header,
							  (UINT16)(sizeof(*header) - sizeof(header->crc)));
}

static UINT16 LogStorage_EntryCrc(const LOG_STORAGE_ENTRY *entry)
{
	return StorageFlash_Crc16((const UINT8 *)entry,
							  (UINT16)(sizeof(*entry) - sizeof(entry->crc)));
}

static UINT8 LogStorage_ReadHeader(UINT32 page, LOG_STORAGE_PAGE_HEADER *header)
{
	const LOG_STORAGE_PAGE_HEADER *stored;

	if (header == 0)
	{
		return 0U;
	}

	stored = (const LOG_STORAGE_PAGE_HEADER *)page;
	if ((stored->magic != LOG_STORAGE_PAGE_MAGIC) ||
		((stored->flags & (UINT16)~LOG_STORAGE_PAGE_FLAGS_VALID) != 0U) ||
		(stored->crc != LogStorage_HeaderCrc(stored)))
	{
		return 0U;
	}

	*header = *stored;
	return 1U;
}

static UINT8 LogStorage_ReadEntry(UINT32 addr, LOG_STORAGE_ENTRY *entry)
{
	const LOG_STORAGE_ENTRY *stored;

	if (entry == 0)
	{
		return 0U;
	}

	stored = (const LOG_STORAGE_ENTRY *)addr;
	if ((stored->crc != LogStorage_EntryCrc(stored)) ||
		!LogRecord_IsEntryValid(stored->event, stored->delta))
	{
		return 0U;
	}

	*entry = *stored;
	return 1U;
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

static UINT8 LogStorage_GetPageInfo(UINT32 page, LOG_STORAGE_PAGE_INFO *info)
{
	LOG_STORAGE_PAGE_HEADER header;
	UINT32 addr;
	UINT32 endAddr = page + FLASH_STORAGE_PAGE_SIZE;

	if (info == 0)
	{
		return 0U;
	}

	memset(info, 0, sizeof(*info));
	info->page = page;
	if (!LogStorage_ReadHeader(page, &header))
	{
		return 0U;
	}

	info->valid = 1U;
	info->flags = header.flags;
	info->generation = header.generation;
	info->nextAddr = endAddr;

	for (addr = page + sizeof(LOG_STORAGE_PAGE_HEADER);
		 (addr + sizeof(LOG_STORAGE_ENTRY)) <= endAddr;
		 addr += sizeof(LOG_STORAGE_ENTRY))
	{
		if (LogStorage_IsBlank(addr, (UINT16)sizeof(LOG_STORAGE_ENTRY)))
		{
			info->nextAddr = addr;
			break;
		}
	}
	return 1U;
}

static UINT8 LogStorage_ReplayPage(const LOG_STORAGE_PAGE_INFO *info,
									UINT8 *point,
									UINT8 records[EVENT_RECORD_LENGTH][2])
{
	UINT32 addr;
	UINT32 endAddr;
	LOG_STORAGE_ENTRY entry;

	if ((info == 0) || !info->valid)
	{
		return 0U;
	}

	endAddr = info->page + FLASH_STORAGE_PAGE_SIZE;
	for (addr = info->page + sizeof(LOG_STORAGE_PAGE_HEADER);
		 (addr + sizeof(LOG_STORAGE_ENTRY)) <= endAddr;
		 addr += sizeof(LOG_STORAGE_ENTRY))
	{
		if (LogStorage_IsBlank(addr, (UINT16)sizeof(LOG_STORAGE_ENTRY)))
		{
			break;
		}

		/* A torn write consumes one slot but must not invalidate older records.
		 * Subsequent boot writes continue at the first blank slot after it. */
		if (!LogStorage_ReadEntry(addr, &entry))
		{
			continue;
		}
		LogStorage_ApplyEntry(point, records, entry.event, entry.delta);
	}
	return 1U;
}

static UINT8 LogStorage_StartPage(UINT32 page, UINT32 generation, UINT16 flags)
{
	LOG_STORAGE_PAGE_HEADER header;
	LOG_STORAGE_PAGE_HEADER verify;

	if (!StorageFlash_EraseStoragePage(page))
	{
		return 0U;
	}

	header.magic = LOG_STORAGE_PAGE_MAGIC;
	header.generation = generation;
	header.flags = flags;
	header.crc = LogStorage_HeaderCrc(&header);

	if (!StorageFlash_ProgramStorageBytes(page,
									 (const UINT8 *)&header,
									 (UINT16)sizeof(header)))
	{
		return 0U;
	}
	if (!LogStorage_ReadHeader(page, &verify) ||
		(verify.generation != generation) ||
		(verify.flags != flags))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}

	s_log_record.storagePage = page;
	s_log_record.storageGeneration = generation;
	s_log_record.storageNextAddr = page + sizeof(LOG_STORAGE_PAGE_HEADER);
	return 1U;
}

static UINT8 LogStorage_Load(void)
{
	LOG_STORAGE_PAGE_INFO pageA;
	LOG_STORAGE_PAGE_INFO pageB;
	LOG_STORAGE_PAGE_INFO *newest;
	LOG_STORAGE_PAGE_INFO *older;
	UINT8 validA;
	UINT8 validB;

	validA = LogStorage_GetPageInfo(FLASH_ADDR_STORAGE_LOG_SLOT_A, &pageA);
	validB = LogStorage_GetPageInfo(FLASH_ADDR_STORAGE_LOG_SLOT_B, &pageB);
	if (!validA && !validB)
	{
		return 0U;
	}

	if (validA && validB)
	{
		if (pageA.generation >= pageB.generation)
		{
			newest = &pageA;
			older = &pageB;
		}
		else
		{
			newest = &pageB;
			older = &pageA;
		}
	}
	else if (validA)
	{
		newest = &pageA;
		older = 0;
	}
	else
	{
		newest = &pageB;
		older = 0;
	}

	memset(s_log_record.records, 0, sizeof(s_log_record.records));
	s_log_record.point = 0U;

	/* A committed reset page is the new history root. Older generations must
	 * never be replayed even if cleanup was interrupted by power loss. */
	if (((newest->flags & LOG_STORAGE_PAGE_FLAG_RESET) == 0U) && (older != 0))
	{
		(void)LogStorage_ReplayPage(older, &s_log_record.point, s_log_record.records);
	}
	(void)LogStorage_ReplayPage(newest, &s_log_record.point, s_log_record.records);

	s_log_record.storagePage = newest->page;
	s_log_record.storageGeneration = newest->generation;
	s_log_record.storageNextAddr = newest->nextAddr;
	return 1U;
}

static UINT8 LogStorage_Reset(void)
{
	LOG_STORAGE_PAGE_INFO pageA;
	LOG_STORAGE_PAGE_INFO pageB;
	UINT8 validA;
	UINT8 validB;
	UINT32 targetPage = FLASH_ADDR_STORAGE_LOG_SLOT_A;
	UINT32 stalePage = FLASH_ADDR_STORAGE_LOG_SLOT_B;
	UINT32 generation = 1U;
	UINT8 cleanupOk;

	validA = LogStorage_GetPageInfo(FLASH_ADDR_STORAGE_LOG_SLOT_A, &pageA);
	validB = LogStorage_GetPageInfo(FLASH_ADDR_STORAGE_LOG_SLOT_B, &pageB);

	if (validA || validB)
	{
		if (validA && (!validB || (pageA.generation >= pageB.generation)))
		{
			generation = pageA.generation + 1U;
			targetPage = FLASH_ADDR_STORAGE_LOG_SLOT_B;
			stalePage = FLASH_ADDR_STORAGE_LOG_SLOT_A;
		}
		else
		{
			generation = pageB.generation + 1U;
			targetPage = FLASH_ADDR_STORAGE_LOG_SLOT_A;
			stalePage = FLASH_ADDR_STORAGE_LOG_SLOT_B;
		}
		if (generation == 0U)
		{
			generation = 1U;
		}
	}

	/* Commit the reset marker before erasing the previous history. */
	if (!LogStorage_StartPage(targetPage, generation, LOG_STORAGE_PAGE_FLAG_RESET))
	{
		return 0U;
	}

	cleanupOk = StorageFlash_EraseStoragePage(stalePage);
	memset(s_log_record.records, 0, sizeof(s_log_record.records));
	memset(s_log_record.lastSaveValid, 0, sizeof(s_log_record.lastSaveValid));
	memset(s_log_record.eventLatch, 0, sizeof(s_log_record.eventLatch));
	s_log_record.point = 0U;
	s_log_record.cbcTemp = 0U;

	return cleanupOk;
}

static UINT8 LogStorage_Rollover(void)
{
	UINT32 nextPage;
	UINT32 nextGeneration;

	nextPage = (s_log_record.storagePage == FLASH_ADDR_STORAGE_LOG_SLOT_A) ?
			   FLASH_ADDR_STORAGE_LOG_SLOT_B : FLASH_ADDR_STORAGE_LOG_SLOT_A;
	nextGeneration = s_log_record.storageGeneration + 1U;
	if (nextGeneration == 0U)
	{
		nextGeneration = 1U;
	}

	/* The full current page remains valid until the new page header is fully
	 * committed, so power loss during rollover cannot erase the latest log. */
	return LogStorage_StartPage(nextPage, nextGeneration, 0U);
}

static UINT8 LogStorage_PersistEvent(LogEventArray event, UINT8 delta)
{
	LOG_STORAGE_ENTRY entry;
	LOG_STORAGE_ENTRY verify;
	UINT32 pageEnd;

	if ((s_log_record.storagePage != FLASH_ADDR_STORAGE_LOG_SLOT_A) &&
		(s_log_record.storagePage != FLASH_ADDR_STORAGE_LOG_SLOT_B))
	{
		if (!LogStorage_Load() && !LogStorage_Reset())
		{
			return 0U;
		}
	}

	pageEnd = s_log_record.storagePage + FLASH_STORAGE_PAGE_SIZE;
	if ((s_log_record.storageNextAddr + sizeof(LOG_STORAGE_ENTRY)) > pageEnd)
	{
		if (!LogStorage_Rollover())
		{
			return 0U;
		}
	}

	entry.event = (UINT8)event;
	entry.delta = delta;
	entry.crc = LogStorage_EntryCrc(&entry);

	if (!StorageFlash_ProgramStorageBytes(s_log_record.storageNextAddr,
									 (const UINT8 *)&entry,
									 (UINT16)sizeof(entry)))
	{
		return 0U;
	}
	if (!LogStorage_ReadEntry(s_log_record.storageNextAddr, &verify) ||
		(verify.event != entry.event) ||
		(verify.delta != entry.delta))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}

	s_log_record.storageNextAddr += sizeof(LOG_STORAGE_ENTRY);
	return 1U;
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
	if (!LogStorage_Reset())
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}
	return 1U;
}

void ReadEEPROM_EventRecord_Parameters(void)
{
	if (!LogStorage_Load())
	{
		if (!LogStorage_Reset())
		{
			System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		}
	}
}
