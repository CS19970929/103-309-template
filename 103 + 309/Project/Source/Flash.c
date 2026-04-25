#include "main.h"

typedef void (*pFunction)(void);
pFunction Jump_To_Application;
uint32_t JumpAddress;

#define FLASH_STORAGE_MAGIC_SOC ((UINT32)0x534F4331)
#define FLASH_STORAGE_MAGIC_AFE ((UINT32)0x41464531)
#define FLASH_STORAGE_MAGIC_LOG ((UINT32)0x4C4F4731)
#define FLASH_STORAGE_VERSION   ((UINT16)0x0001)

typedef struct
{
	UINT32 magic;
	UINT16 version;
	UINT16 length;
	UINT32 sequence;
	UINT16 crc;
	UINT16 reserved;
} STORAGE_FLASH_HEADER;

typedef struct
{
	UINT8 point;
	UINT8 reserved;
	UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2];
} STORAGE_FLASH_LOG_DATA;

static UINT16 StorageFlash_CalcCrc(const UINT8 *data, UINT16 length)
{
	if ((data == 0) || (length == 0))
	{
		return 0xFFFF;
	}

	return Sci_CRC16RTU((UINT8 *)data, (UINT8)length);
}

static FLASH_Status FlashErasePageVerified(uint32_t page_addr)
{
	UINT32 offset;
	FLASH_Status result;

	result = FLASH_ErasePage(page_addr);
	if (result != FLASH_COMPLETE)
	{
		return result;
	}

	for (offset = 0; offset < FLASH_STORAGE_PAGE_SIZE; offset += 2)
	{
		if (FlashReadOneHalfWord(page_addr + offset) != 0xFFFF)
		{
			return FLASH_ERROR_PG;
		}
	}

	return FLASH_COMPLETE;
}

static FLASH_Status FlashProgramHalfWordVerified(uint32_t addr, uint16_t data)
{
	FLASH_Status result;

	result = FLASH_ProgramHalfWord(addr, data);
	if (result != FLASH_COMPLETE)
	{
		return result;
	}

	if (FlashReadOneHalfWord(addr) != data)
	{
		return FLASH_ERROR_PG;
	}

	return FLASH_COMPLETE;
}

static FLASH_Status FlashProgramBytesVerified(uint32_t addr, const UINT8 *data, UINT16 length)
{
	UINT16 offset = 0;
	UINT16 half_word;
	FLASH_Status result = FLASH_COMPLETE;

	while (offset < length)
	{
		half_word = data[offset];
		if ((offset + 1) < length)
		{
			half_word |= ((UINT16)data[offset + 1] << 8);
		}
		else
		{
			half_word |= 0xFF00;
		}

		result = FlashProgramHalfWordVerified(addr + offset, half_word);
		if (result != FLASH_COMPLETE)
		{
			return result;
		}
		offset += 2;
	}

	return result;
}

static UINT16 StorageFlash_RecordSpan(UINT16 payload_length)
{
	UINT16 record_span;

	record_span = (UINT16)(sizeof(STORAGE_FLASH_HEADER) + payload_length);
	if (record_span & 0x0001)
	{
		record_span += 1;
	}

	return record_span;
}

static UINT8 StorageFlash_IsAreaBlank(uint32_t addr, UINT16 length)
{
	UINT16 offset;

	for (offset = 0; offset < length; offset += 2)
	{
		if (FlashReadOneHalfWord(addr + offset) != 0xFFFF)
		{
			return 0;
		}
	}

	return 1;
}

static UINT8 StorageFlash_ReadSlot(uint32_t slot_addr,
								   UINT32 expect_magic,
								   UINT16 expect_length,
								   UINT8 *payload,
								   UINT32 *sequence)
{
	const STORAGE_FLASH_HEADER *header = (const STORAGE_FLASH_HEADER *)slot_addr;
	const UINT8 *payload_addr = (const UINT8 *)(slot_addr + sizeof(STORAGE_FLASH_HEADER));
	UINT16 crc;

	if ((header->magic != expect_magic) || (header->version != FLASH_STORAGE_VERSION) || (header->length != expect_length))
	{
		return 0;
	}

	crc = StorageFlash_CalcCrc(payload_addr, expect_length);
	if (crc != header->crc)
	{
		return 0;
	}

	if (payload != 0)
	{
		memcpy(payload, payload_addr, expect_length);
	}

	if (sequence != 0)
	{
		*sequence = header->sequence;
	}

	return 1;
}

static FLASH_Status StorageFlash_ProgramRecord(uint32_t record_addr,
											   UINT32 magic,
											   const UINT8 *payload,
											   UINT16 length,
											   UINT32 sequence)
{
	STORAGE_FLASH_HEADER header;
	FLASH_Status result;

	header.magic = magic;
	header.version = FLASH_STORAGE_VERSION;
	header.length = length;
	header.sequence = sequence;
	header.crc = StorageFlash_CalcCrc(payload, length);
	header.reserved = 0xFFFF;

	result = FlashProgramBytesVerified(record_addr, (const UINT8 *)&header, (UINT16)sizeof(header));
	if (result == FLASH_COMPLETE)
	{
		result = FlashProgramBytesVerified(record_addr + sizeof(header), payload, length);
	}

	return result;
}

static UINT8 StorageFlash_LoadPair(uint32_t slot_a,
								   uint32_t slot_b,
								   UINT32 expect_magic,
								   UINT16 expect_length,
								   UINT8 *payload)
{
	UINT8 buffer_a[256];
	UINT8 buffer_b[256];
	UINT8 *chosen = 0;
	UINT32 seq_a = 0;
	UINT32 seq_b = 0;
	UINT8 valid_a;
	UINT8 valid_b;

	if (expect_length > sizeof(buffer_a))
	{
		return 0;
	}

	valid_a = StorageFlash_ReadSlot(slot_a, expect_magic, expect_length, buffer_a, &seq_a);
	valid_b = StorageFlash_ReadSlot(slot_b, expect_magic, expect_length, buffer_b, &seq_b);

	if (!valid_a && !valid_b)
	{
		return 0;
	}

	if (valid_a && valid_b)
	{
		chosen = (seq_a >= seq_b) ? buffer_a : buffer_b;
	}
	else if (valid_a)
	{
		chosen = buffer_a;
	}
	else
	{
		chosen = buffer_b;
	}

	memcpy(payload, chosen, expect_length);
	return 1;
}

static FLASH_Status StorageFlash_WriteSlot(uint32_t slot_addr,
								   UINT32 magic,
								   const UINT8 *payload,
								   UINT16 length,
								   UINT32 sequence)
{
	FLASH_Status result;

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

	result = FlashErasePageVerified(slot_addr);
	if (result == FLASH_COMPLETE)
	{
		result = StorageFlash_ProgramRecord(slot_addr, magic, payload, length, sequence);
	}

	FLASH_Lock();
	return result;
}

static UINT8 StorageFlash_SavePair(uint32_t slot_a,
								   uint32_t slot_b,
								   UINT32 magic,
								   const UINT8 *payload,
								   UINT16 length)
{
	UINT8 verify_buffer[256];
	UINT8 valid_a;
	UINT8 valid_b;
	UINT32 seq_a = 0;
	UINT32 seq_b = 0;
	UINT32 next_sequence = 1;
	uint32_t target_slot = slot_a;
	FLASH_Status result;

	if (length > sizeof(verify_buffer))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	valid_a = StorageFlash_ReadSlot(slot_a, magic, length, verify_buffer, &seq_a);
	valid_b = StorageFlash_ReadSlot(slot_b, magic, length, verify_buffer, &seq_b);

	if (valid_a && valid_b)
	{
		if (seq_a >= seq_b)
		{
			next_sequence = seq_a + 1;
			target_slot = slot_b;
		}
		else
		{
			next_sequence = seq_b + 1;
			target_slot = slot_a;
		}
	}
	else if (valid_a)
	{
		next_sequence = seq_a + 1;
		target_slot = slot_b;
	}
	else if (valid_b)
	{
		next_sequence = seq_b + 1;
		target_slot = slot_a;
	}

	result = StorageFlash_WriteSlot(target_slot, magic, payload, length, next_sequence);
	if (result != FLASH_COMPLETE)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	if (!StorageFlash_ReadSlot(target_slot, magic, length, verify_buffer, &seq_a))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	return 1;
}

static UINT8 StorageFlash_LoadJournalPage(uint32_t slot_addr,
										  UINT32 expect_magic,
										  UINT16 expect_length,
										  UINT8 *payload,
										  UINT32 *sequence,
										  UINT32 *next_addr)
{
	UINT8 temp_payload[256];
	UINT16 record_span;
	UINT32 offset;
	UINT32 record_addr;
	UINT32 latest_sequence = 0;
	UINT32 current_sequence = 0;
	UINT32 blank_addr = slot_addr + FLASH_STORAGE_PAGE_SIZE;
	UINT8 found = 0;

	if ((payload != 0) && (expect_length > sizeof(temp_payload)))
	{
		return 0;
	}

	record_span = StorageFlash_RecordSpan(expect_length);
	if (record_span > FLASH_STORAGE_PAGE_SIZE)
	{
		return 0;
	}

	for (offset = 0; (offset + record_span) <= FLASH_STORAGE_PAGE_SIZE; offset += record_span)
	{
		record_addr = slot_addr + offset;

		if (StorageFlash_IsAreaBlank(record_addr, record_span))
		{
			blank_addr = record_addr;
			break;
		}

		if (StorageFlash_ReadSlot(record_addr,
								  expect_magic,
								  expect_length,
								  payload != 0 ? temp_payload : 0,
								  &current_sequence))
		{
			if ((!found) || (current_sequence >= latest_sequence))
			{
				latest_sequence = current_sequence;
				found = 1;
				if (payload != 0)
				{
					memcpy(payload, temp_payload, expect_length);
				}
			}
		}
	}

	if (sequence != 0)
	{
		*sequence = found ? latest_sequence : 0;
	}

	if (next_addr != 0)
	{
		*next_addr = blank_addr;
	}

	return found;
}

static UINT8 StorageFlash_LoadJournalPair(uint32_t slot_a,
										  uint32_t slot_b,
										  UINT32 expect_magic,
										  UINT16 expect_length,
										  UINT8 *payload)
{
	UINT8 buffer_a[256];
	UINT8 buffer_b[256];
	UINT8 *chosen = 0;
	UINT32 seq_a = 0;
	UINT32 seq_b = 0;
	UINT8 valid_a;
	UINT8 valid_b;

	if (expect_length > sizeof(buffer_a))
	{
		return 0;
	}

	valid_a = StorageFlash_LoadJournalPage(slot_a, expect_magic, expect_length, buffer_a, &seq_a, 0);
	valid_b = StorageFlash_LoadJournalPage(slot_b, expect_magic, expect_length, buffer_b, &seq_b, 0);

	if (!valid_a && !valid_b)
	{
		return 0;
	}

	if (valid_a && valid_b)
	{
		chosen = (seq_a >= seq_b) ? buffer_a : buffer_b;
	}
	else if (valid_a)
	{
		chosen = buffer_a;
	}
	else
	{
		chosen = buffer_b;
	}

	memcpy(payload, chosen, expect_length);
	return 1;
}

static UINT8 StorageFlash_SaveJournalPair(uint32_t slot_a,
										  uint32_t slot_b,
										  UINT32 magic,
										  const UINT8 *payload,
										  UINT16 length)
{
	UINT8 verify_buffer[256];
	UINT16 record_span;
	UINT8 valid_a;
	UINT8 valid_b;
	UINT32 seq_a = 0;
	UINT32 seq_b = 0;
	UINT32 next_addr_a = slot_a;
	UINT32 next_addr_b = slot_b;
	UINT32 next_sequence = 1;
	UINT32 target_page = slot_a;
	UINT32 target_addr = slot_a;
	UINT32 verify_sequence = 0;
	FLASH_Status result;
	UINT8 erase_target_page = 0;

	if (length > sizeof(verify_buffer))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	record_span = StorageFlash_RecordSpan(length);
	if (record_span > FLASH_STORAGE_PAGE_SIZE)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	valid_a = StorageFlash_LoadJournalPage(slot_a, magic, length, 0, &seq_a, &next_addr_a);
	valid_b = StorageFlash_LoadJournalPage(slot_b, magic, length, 0, &seq_b, &next_addr_b);

	if (valid_a && valid_b)
	{
		if (seq_a >= seq_b)
		{
			next_sequence = seq_a + 1;
			target_page = slot_a;
			target_addr = next_addr_a;
		}
		else
		{
			next_sequence = seq_b + 1;
			target_page = slot_b;
			target_addr = next_addr_b;
		}
	}
	else if (valid_a)
	{
		next_sequence = seq_a + 1;
		target_page = slot_a;
		target_addr = next_addr_a;
	}
	else if (valid_b)
	{
		next_sequence = seq_b + 1;
		target_page = slot_b;
		target_addr = next_addr_b;
	}
	else
	{
		erase_target_page = 1;
	}

	if ((target_addr + record_span) > (target_page + FLASH_STORAGE_PAGE_SIZE))
	{
		target_page = (target_page == slot_a) ? slot_b : slot_a;
		target_addr = target_page;
		erase_target_page = 1;
	}

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

	if (erase_target_page)
	{
		result = FlashErasePageVerified(target_page);
		if (result != FLASH_COMPLETE)
		{
			FLASH_Lock();
			System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			return 0;
		}
	}

	result = StorageFlash_ProgramRecord(target_addr, magic, payload, length, next_sequence);
	FLASH_Lock();
	if (result != FLASH_COMPLETE)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	if (!StorageFlash_ReadSlot(target_addr, magic, length, verify_buffer, &verify_sequence))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	if ((verify_sequence != next_sequence) || (memcmp(verify_buffer, payload, length) != 0))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	return 1;
}

FLASH_Status FlashWriteOneHalfWord(uint32_t StartAddr, uint16_t Buffer)
{
	FLASH_Status result;

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	while (FlashErasePageVerified(StartAddr) != FLASH_COMPLETE)
	{
	}
	result = FlashProgramHalfWordVerified(StartAddr, Buffer);
	FLASH_Lock();
	return result;
}

UINT16 FlashReadOneHalfWord(UINT32 faddr)
{
	return *(vu16 *)faddr;
}

void FlashTest(void)
{
	g_stCellInfoReport.u16VCell[2] = FlashReadOneHalfWord(FLASH_ADDR_UPDATE_FLAG);
}

UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data)
{
	if (data == 0)
	{
		return 0;
	}

	return StorageFlash_LoadJournalPair(FLASH_ADDR_STORAGE_SOC_SLOT_A,
										FLASH_ADDR_STORAGE_SOC_SLOT_B,
										FLASH_STORAGE_MAGIC_SOC,
										(UINT16)sizeof(STORAGE_FLASH_SOC_DATA),
										(UINT8 *)data);
}

UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data)
{
	if (data == 0)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	return StorageFlash_SaveJournalPair(FLASH_ADDR_STORAGE_SOC_SLOT_A,
										FLASH_ADDR_STORAGE_SOC_SLOT_B,
										FLASH_STORAGE_MAGIC_SOC,
										(const UINT8 *)data,
										(UINT16)sizeof(STORAGE_FLASH_SOC_DATA));
}

UINT8 StorageFlash_LoadAfeData(UINT16 *values, UINT16 word_count)
{
	if ((values == 0) || (word_count != FLASH_STORAGE_AFE_WORD_COUNT))
	{
		return 0;
	}

	return StorageFlash_LoadPair(FLASH_ADDR_STORAGE_AFE_SLOT_A,
								 FLASH_ADDR_STORAGE_AFE_SLOT_B,
								 FLASH_STORAGE_MAGIC_AFE,
								 (UINT16)(word_count * sizeof(UINT16)),
								 (UINT8 *)values);
}

UINT8 StorageFlash_SaveAfeData(const UINT16 *values, UINT16 word_count)
{
	if ((values == 0) || (word_count != FLASH_STORAGE_AFE_WORD_COUNT))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	return StorageFlash_SavePair(FLASH_ADDR_STORAGE_AFE_SLOT_A,
								 FLASH_ADDR_STORAGE_AFE_SLOT_B,
								 FLASH_STORAGE_MAGIC_AFE,
								 (const UINT8 *)values,
								 (UINT16)(word_count * sizeof(UINT16)));
}

UINT8 StorageFlash_LoadLogData(UINT8 *point, UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2])
{
	STORAGE_FLASH_LOG_DATA data;

	if ((point == 0) || (records == 0))
	{
		return 0;
	}

	if (!StorageFlash_LoadJournalPair(FLASH_ADDR_STORAGE_LOG_SLOT_A,
									  FLASH_ADDR_STORAGE_LOG_SLOT_B,
									  FLASH_STORAGE_MAGIC_LOG,
									  (UINT16)sizeof(STORAGE_FLASH_LOG_DATA),
									  (UINT8 *)&data))
	{
		return 0;
	}

	*point = data.point;
	memcpy(records, data.records, sizeof(data.records));
	return 1;
}

UINT8 StorageFlash_SaveLogData(UINT8 point, const UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2])
{
	STORAGE_FLASH_LOG_DATA data;

	if (records == 0)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	memset(&data, 0, sizeof(data));
	data.point = point;
	memcpy(data.records, records, sizeof(data.records));

	return StorageFlash_SaveJournalPair(FLASH_ADDR_STORAGE_LOG_SLOT_A,
										FLASH_ADDR_STORAGE_LOG_SLOT_B,
										FLASH_STORAGE_MAGIC_LOG,
										(const UINT8 *)&data,
										(UINT16)sizeof(STORAGE_FLASH_LOG_DATA));
}

void App_FlashUpdate(void)
{
#ifdef _IAP
	if (1 == u8FlashUpdateFlag)
	{
		SH367309_DriverMos_Ctrl(GPIO_CHG, 0);
		SH367309_DriverMos_Ctrl(GPIO_DSG, 0);
		__delay_ms(10);
		u8FlashUpdateFlag = 0;
		__disable_fault_irq();
		MCU_RESET();
	}
#endif
}

void APP_To_IAP_Jump(void)
{
	if (((*(__IO uint32_t *)FLASH_ADDR_IAP_START) & 0x2FFE0000) == 0x20000000)
	{
		JumpAddress = *(__IO uint32_t *)(FLASH_ADDR_IAP_START + 4);
		Jump_To_Application = (pFunction)JumpAddress;
		__set_MSP(*(__IO uint32_t *)FLASH_ADDR_IAP_START);
		Jump_To_Application();
	}
}

void InitAreaSelect(void)
{
	if (FlashReadOneHalfWord(FLASH_ADDR_UPDATE_FLAG) == FLASH_TO_IAP_VALUE)
	{
		APP_To_IAP_Jump();
	}
}
