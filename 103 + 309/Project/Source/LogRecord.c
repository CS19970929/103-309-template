#include "main.h"

#define EVENT_RECORD_LENGTH                  FLASH_STORAGE_LOG_RECORD_COUNT
#define LOG_STORAGE_PAGE_MAGIC               ((UINT32)0x4C4F4733U) /* LOG3: keep old A/B compatible */
#define LOG_STORAGE_PAGE_FLAG_RESET          ((UINT16)0x0001U)
#define LOG_STORAGE_PAGE_FLAGS_VALID         LOG_STORAGE_PAGE_FLAG_RESET

typedef struct LOG_RECORD_RUNTIME_TAG
{
	UINT16 point;
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

#define LOG_STORAGE_ENTRY_CAPACITY_PER_PAGE \
	((FLASH_STORAGE_PAGE_SIZE - sizeof(LOG_STORAGE_PAGE_HEADER)) / sizeof(LOG_STORAGE_ENTRY))

/* During rollover one page may be erased or contain a torn header. The other
 * three pages must still be able to retain the complete 500-record history. */
typedef char LogStorageRetainedCapacityCheck[
	((((FLASH_STORAGE_LOG_PAGE_COUNT - 1U) * LOG_STORAGE_ENTRY_CAPACITY_PER_PAGE) >=
	  EVENT_RECORD_LENGTH) ? 1 : -1)];

typedef char LogStoragePageCountCheck[(FLASH_STORAGE_LOG_PAGE_COUNT == 4U) ? 1 : -1];

static const UINT32 s_log_storage_pages[FLASH_STORAGE_LOG_PAGE_COUNT] = {
	FLASH_ADDR_STORAGE_LOG_SLOT_A,
	FLASH_ADDR_STORAGE_LOG_SLOT_B,
	FLASH_ADDR_STORAGE_LOG_SLOT_C,
	FLASH_ADDR_STORAGE_LOG_SLOT_D};

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

static void LogStorage_ApplyEntry(UINT16 *point,
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
	if (*point >= EVENT_RECORD_LENGTH)
	{
		*point = 0U;
	}
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
									UINT16 *point,
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

		/* A torn entry consumes its non-blank slot but does not invalidate any
		 * older or newer valid entry in this page. */
		if (!LogStorage_ReadEntry(addr, &entry))
		{
			continue;
		}
		LogStorage_ApplyEntry(point, records, entry.event, entry.delta);
	}
	return 1U;
}

static UINT8 LogStorage_GenerationIsNewer(UINT32 a, UINT32 b)
{
	if (a == b)
	{
		return 0U;
	}
	return (((UINT32)(a - b)) < 0x80000000UL) ? 1U : 0U;
}

static void LogStorage_SortOldestFirst(LOG_STORAGE_PAGE_INFO pages[], UINT8 count)
{
	UINT8 i;
	UINT8 j;
	LOG_STORAGE_PAGE_INFO temp;

	for (i = 0U; i < count; ++i)
	{
		for (j = (UINT8)(i + 1U); j < count; ++j)
		{
			if (LogStorage_GenerationIsNewer(pages[i].generation, pages[j].generation))
			{
				temp = pages[i];
				pages[i] = pages[j];
				pages[j] = temp;
			}
		}
	}
}

static UINT8 LogStorage_CollectPages(LOG_STORAGE_PAGE_INFO pages[])
{
	UINT8 i;
	UINT8 count = 0U;
	LOG_STORAGE_PAGE_INFO info;

	for (i = 0U; i < FLASH_STORAGE_LOG_PAGE_COUNT; ++i)
	{
		if (LogStorage_GetPageInfo(s_log_storage_pages[i], &info))
		{
			pages[count++] = info;
		}
	}
	LogStorage_SortOldestFirst(pages, count);
	return count;
}

static UINT8 LogStorage_PageIsInList(const LOG_STORAGE_PAGE_INFO pages[],
									  UINT8 count,
									  UINT32 page)
{
	UINT8 i;

	for (i = 0U; i < count; ++i)
	{
		if (pages[i].page == page)
		{
			return 1U;
		}
	}
	return 0U;
}

static UINT8 LogStorage_IsKnownPage(UINT32 page)
{
	UINT8 i;

	for (i = 0U; i < FLASH_STORAGE_LOG_PAGE_COUNT; ++i)
	{
		if (s_log_storage_pages[i] == page)
		{
			return 1U;
		}
	}
	return 0U;
}

static UINT8 LogStorage_StartPage(UINT32 page, UINT32 generation, UINT16 flags)
{
	LOG_STORAGE_PAGE_HEADER header;
	LOG_STORAGE_PAGE_HEADER verify;

	/* Fresh/previously-cleaned pages do not need another erase. A non-blank
	 * target is always erased and verified before its new generation is written. */
	if (!LogStorage_IsBlank(page, (UINT16)FLASH_STORAGE_PAGE_SIZE) &&
		!StorageFlash_EraseStoragePage(page))
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
	LOG_STORAGE_PAGE_INFO pages[FLASH_STORAGE_LOG_PAGE_COUNT];
	LOG_STORAGE_PAGE_INFO *newest;
	UINT8 count;
	UINT8 replayStart = 0U;
	UINT8 i;

	count = LogStorage_CollectPages(pages);
	if (count == 0U)
	{
		return 0U;
	}

	/* The newest RESET generation is the history root. Older pages can remain
	 * physically present after an interrupted reset and are intentionally ignored. */
	for (i = 0U; i < count; ++i)
	{
		if ((pages[i].flags & LOG_STORAGE_PAGE_FLAG_RESET) != 0U)
		{
			replayStart = i;
		}
	}

	memset(s_log_record.records, 0, sizeof(s_log_record.records));
	s_log_record.point = 0U;
	for (i = replayStart; i < count; ++i)
	{
		(void)LogStorage_ReplayPage(&pages[i], &s_log_record.point, s_log_record.records);
	}

	newest = &pages[count - 1U];
	s_log_record.storagePage = newest->page;
	s_log_record.storageGeneration = newest->generation;
	s_log_record.storageNextAddr = newest->nextAddr;
	return 1U;
}

static UINT32 LogStorage_SelectReusablePage(const LOG_STORAGE_PAGE_INFO pages[],
										 UINT8 count,
										 UINT32 excludePage)
{
	UINT8 i;

	/* Prefer a page with no valid header. This makes migration from the old
	 * two-page LOG3 layout use the new pages before erasing old history. */
	for (i = 0U; i < FLASH_STORAGE_LOG_PAGE_COUNT; ++i)
	{
		if ((s_log_storage_pages[i] != excludePage) &&
			!LogStorage_PageIsInList(pages, count, s_log_storage_pages[i]))
		{
			return s_log_storage_pages[i];
		}
	}

	/* All pages are valid: pages[] is oldest-to-newest, so erase the oldest
	 * generation that is not the current active page. */
	for (i = 0U; i < count; ++i)
	{
		if (pages[i].page != excludePage)
		{
			return pages[i].page;
		}
	}
	return 0U;
}

static UINT8 LogStorage_Reset(void)
{
	LOG_STORAGE_PAGE_INFO pages[FLASH_STORAGE_LOG_PAGE_COUNT];
	UINT8 count;
	UINT32 targetPage;
	UINT32 generation = 1U;

	count = LogStorage_CollectPages(pages);
	if (count != 0U)
	{
		generation = pages[count - 1U].generation + 1U;
		if (generation == 0U)
		{
			generation = 1U;
		}
	}

	targetPage = LogStorage_SelectReusablePage(pages, count, 0U);
	if (targetPage == 0U)
	{
		return 0U;
	}

	/* The reset marker is the atomic commit. Old pages are deliberately not
	 * erased here: this avoids two unnecessary erase cycles and guarantees that
	 * a power loss anywhere before the new header commits leaves old history
	 * intact. Future rollovers reclaim those pages oldest-first. */
	if (!LogStorage_StartPage(targetPage, generation, LOG_STORAGE_PAGE_FLAG_RESET))
	{
		return 0U;
	}

	memset(s_log_record.records, 0, sizeof(s_log_record.records));
	memset(s_log_record.lastSaveValid, 0, sizeof(s_log_record.lastSaveValid));
	memset(s_log_record.eventLatch, 0, sizeof(s_log_record.eventLatch));
	s_log_record.point = 0U;
	s_log_record.cbcTemp = 0U;
	return 1U;
}

static UINT8 LogStorage_Rollover(void)
{
	LOG_STORAGE_PAGE_INFO pages[FLASH_STORAGE_LOG_PAGE_COUNT];
	UINT8 count;
	UINT32 nextPage;
	UINT32 nextGeneration;

	count = LogStorage_CollectPages(pages);
	nextPage = LogStorage_SelectReusablePage(pages, count, s_log_record.storagePage);
	if (nextPage == 0U)
	{
		return 0U;
	}

	nextGeneration = s_log_record.storageGeneration + 1U;
	if (nextGeneration == 0U)
	{
		nextGeneration = 1U;
	}

	/* The current page and at least two additional pages remain valid while the
	 * target page is erased/programmed. With 253 entries/page, three retained
	 * pages provide 759 physical slots for a 500-record logical history. */
	return LogStorage_StartPage(nextPage, nextGeneration, 0U);
}

static UINT8 LogStorage_PersistEvent(LogEventArray event, UINT8 delta)
{
	LOG_STORAGE_ENTRY entry;
	LOG_STORAGE_ENTRY verify;
	UINT32 pageEnd;
	UINT32 writeAddr;
	UINT8 programmed;

	if (!LogStorage_IsKnownPage(s_log_record.storagePage))
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
	writeAddr = s_log_record.storageNextAddr;

	programmed = StorageFlash_ProgramStorageBytes(writeAddr,
											   (const UINT8 *)&entry,
											   (UINT16)sizeof(entry));
	if (LogStorage_ReadEntry(writeAddr, &verify) &&
		(verify.event == entry.event) &&
		(verify.delta == entry.delta))
	{
		/* Treat a fully verified record as committed even if the low-level Flash
		 * API reported an EOP anomaly after programming. */
		s_log_record.storageNextAddr += sizeof(LOG_STORAGE_ENTRY);
		return 1U;
	}

	/* A partially programmed non-blank slot can never be restored to 0xFFFF
	 * without erasing the page. Consume it so later events can continue. If the
	 * failed slot is still blank, retry the same address on the next event; this
	 * avoids creating a blank hole that boot replay would interpret as end-of-log. */
	if (!LogStorage_IsBlank(writeAddr, (UINT16)sizeof(LOG_STORAGE_ENTRY)))
	{
		s_log_record.storageNextAddr += sizeof(LOG_STORAGE_ENTRY);
	}
	(void)programmed;
	System_ERROR_UserCallback(ERROR_EEPROM_STORE);
	return 0U;
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
	UINT16 index;

	for (j = 0U; j < EVENT_RECORD_LENGTH; ++j)
	{
		/* point is always the next write position. Keep newest-first protocol
		 * ordering while using 16-bit arithmetic for the 500-entry ring. */
		index = (UINT16)(s_log_record.point + EVENT_RECORD_LENGTH - 1U - j);
		if (index >= EVENT_RECORD_LENGTH)
		{
			index = (UINT16)(index - EVENT_RECORD_LENGTH);
		}
		t_u8BuffTemp[i++] = s_log_record.records[index][0];
		t_u8BuffTemp[i++] = s_log_record.records[index][1];
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
