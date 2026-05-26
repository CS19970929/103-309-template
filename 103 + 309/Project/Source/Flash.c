#include "main.h"
#include "Flash64KAppTest.h"

typedef void (*pFunction)(void);
pFunction Jump_To_Application;
uint32_t JumpAddress;

#define FLASH_STORAGE_MAGIC_SOC ((UINT32)0x534F4331)
#define FLASH_STORAGE_MAGIC_AFE ((UINT32)0x41464531)
#define FLASH_STORAGE_MAGIC_RW_PARAM ((UINT32)0x52575031)
#define FLASH_STORAGE_MAGIC_LOG ((UINT32)0x4C4F4731)
#define FLASH_STORAGE_VERSION   ((UINT16)0x0001)
#define FLASH_SIZE_REG_ADDR     ((UINT32)0x1FFFF7E0)
#define APP_UPGRADE_MAILBOX_ADDR ((UINT32)0x20004FE0)
#define APP_UPGRADE_MAILBOX_MAGIC ((UINT32)0x49415031)
#define APP_UPGRADE_MAILBOX_REQUEST ((UINT32)0x5AA55AA5)

typedef struct
{
	UINT16 u16SocNow;
	UINT16 u16DsgSocInt;
	UINT32 u32CycleTimes;
} STORAGE_FLASH_SOC_DATA_V1;

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

typedef struct
{
	UINT32 magic;
	UINT32 magic_inv;
	UINT32 request;
	UINT32 request_inv;
	UINT32 crc;
} APP_UPGRADE_MAILBOX;

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

static volatile APP_UPGRADE_MAILBOX *AppUpgrade_Mailbox(void)
{
	return (volatile APP_UPGRADE_MAILBOX *)APP_UPGRADE_MAILBOX_ADDR;
}

static UINT32 AppUpgrade_MailboxCrc(UINT32 magic, UINT32 request)
{
	return magic ^ request ^ 0xA5A55A5A;
}

static UINT8 AppUpgrade_IsIapRequested(void)
{
	volatile APP_UPGRADE_MAILBOX *mailbox;

	mailbox = AppUpgrade_Mailbox();
	if ((mailbox->magic != APP_UPGRADE_MAILBOX_MAGIC) ||
		(mailbox->magic_inv != (UINT32)~APP_UPGRADE_MAILBOX_MAGIC) ||
		(mailbox->request != APP_UPGRADE_MAILBOX_REQUEST) ||
		(mailbox->request_inv != (UINT32)~APP_UPGRADE_MAILBOX_REQUEST) ||
		(mailbox->crc != AppUpgrade_MailboxCrc(APP_UPGRADE_MAILBOX_MAGIC, APP_UPGRADE_MAILBOX_REQUEST)))
	{
		return 0U;
	}

	return 1U;
}

UINT8 AppUpgrade_RequestIap(void)
{
	volatile APP_UPGRADE_MAILBOX *mailbox;

	mailbox = AppUpgrade_Mailbox();
	mailbox->magic = APP_UPGRADE_MAILBOX_MAGIC;
	mailbox->magic_inv = (UINT32)~APP_UPGRADE_MAILBOX_MAGIC;
	mailbox->request = APP_UPGRADE_MAILBOX_REQUEST;
	mailbox->request_inv = (UINT32)~APP_UPGRADE_MAILBOX_REQUEST;
	mailbox->crc = AppUpgrade_MailboxCrc(APP_UPGRADE_MAILBOX_MAGIC, APP_UPGRADE_MAILBOX_REQUEST);

	return AppUpgrade_IsIapRequested();
}

UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data)
{
	STORAGE_FLASH_SOC_DATA_V1 legacy_data;

	if (data == 0)
	{
		return 0;
	}

	if (StorageFlash_LoadJournalPair(FLASH_ADDR_STORAGE_SOC_SLOT_A,
									 FLASH_ADDR_STORAGE_SOC_SLOT_B,
									 FLASH_STORAGE_MAGIC_SOC,
									 (UINT16)sizeof(STORAGE_FLASH_SOC_DATA),
									 (UINT8 *)data))
	{
		if (data->u16FormatVersion == FLASH_STORAGE_SOC_DATA_VERSION_V2)
		{
			return 1;
		}
	}

	if (!StorageFlash_LoadJournalPair(FLASH_ADDR_STORAGE_SOC_SLOT_A,
									  FLASH_ADDR_STORAGE_SOC_SLOT_B,
									  FLASH_STORAGE_MAGIC_SOC,
									  (UINT16)sizeof(STORAGE_FLASH_SOC_DATA_V1),
									  (UINT8 *)&legacy_data))
	{
		return 0;
	}

	memset(data, 0, sizeof(*data));
	data->u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
	data->u16SocNow = legacy_data.u16SocNow;
	data->u16DsgSocInt = legacy_data.u16DsgSocInt;
	data->u32CycleTimes = legacy_data.u32CycleTimes;
	data->u16MaxErrorPercent = 100U;
	return 1;
}

UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data)
{
	STORAGE_FLASH_SOC_DATA save_data;
	UINT8 result;

	if (data == 0)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	save_data = *data;
	save_data.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
	result = StorageFlash_SaveJournalPair(FLASH_ADDR_STORAGE_SOC_SLOT_A,
										  FLASH_ADDR_STORAGE_SOC_SLOT_B,
										  FLASH_STORAGE_MAGIC_SOC,
										  (const UINT8 *)&save_data,
										  (UINT16)sizeof(STORAGE_FLASH_SOC_DATA));
	StorageFlash_AppUseTest_OnSocSaved(&save_data, result);
	return result;
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
	UINT8 result;

	if ((values == 0) || (word_count != FLASH_STORAGE_AFE_WORD_COUNT))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	result = StorageFlash_SavePair(FLASH_ADDR_STORAGE_AFE_SLOT_A,
								   FLASH_ADDR_STORAGE_AFE_SLOT_B,
								   FLASH_STORAGE_MAGIC_AFE,
								   (const UINT8 *)values,
								   (UINT16)(word_count * sizeof(UINT16)));
	StorageFlash_AppUseTest_OnAfeSaved(values, word_count, result);
	return result;
}

UINT8 StorageFlash_LoadRwParamData(STORAGE_FLASH_RW_PARAM_DATA *data)
{
	if (data == 0)
	{
		return 0;
	}

	return StorageFlash_LoadPair(FLASH_ADDR_STORAGE_RW_PARAM_SLOT_A,
								 FLASH_ADDR_STORAGE_RW_PARAM_SLOT_B,
								 FLASH_STORAGE_MAGIC_RW_PARAM,
								 (UINT16)sizeof(STORAGE_FLASH_RW_PARAM_DATA),
								 (UINT8 *)data);
}

UINT8 StorageFlash_SaveRwParamData(const STORAGE_FLASH_RW_PARAM_DATA *data)
{
	if (data == 0)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	return StorageFlash_SavePair(FLASH_ADDR_STORAGE_RW_PARAM_SLOT_A,
								 FLASH_ADDR_STORAGE_RW_PARAM_SLOT_B,
								 FLASH_STORAGE_MAGIC_RW_PARAM,
								 (const UINT8 *)data,
								 (UINT16)sizeof(STORAGE_FLASH_RW_PARAM_DATA));
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

static char StorageFlash_SelectLabel(UINT8 valid_a, UINT32 seq_a, UINT8 valid_b, UINT32 seq_b)
{
	if (valid_a && valid_b)
	{
		return (seq_a >= seq_b) ? 'A' : 'B';
	}
	if (valid_a)
	{
		return 'A';
	}
	if (valid_b)
	{
		return 'B';
	}
	return '-';
}

void StorageFlash_PrintBootCheck(void)
{
	UINT16 flash_size_kb = *((volatile UINT16 *)FLASH_SIZE_REG_ADDR);
	UINT8 afe_valid_a;
	UINT8 afe_valid_b;
	UINT8 rw_valid_a;
	UINT8 rw_valid_b;
	UINT8 soc_valid_a;
	UINT8 soc_valid_b;
	UINT32 afe_seq_a = 0;
	UINT32 afe_seq_b = 0;
	UINT32 rw_seq_a = 0;
	UINT32 rw_seq_b = 0;
	UINT32 soc_seq_a = 0;
	UINT32 soc_seq_b = 0;
	UINT32 soc_next_a = FLASH_ADDR_STORAGE_SOC_SLOT_A;
	UINT32 soc_next_b = FLASH_ADDR_STORAGE_SOC_SLOT_B;
	UINT16 update_flag = 0xFFFF;
	UINT16 upgrade_flag = 0xFFFF;

	printf("\r\n[FLASH_BOOT] flash_size_reg=%uKB page=%lu\r\n",
		   flash_size_kb,
		   (unsigned long)FLASH_STORAGE_PAGE_SIZE);

	if (flash_size_kb < 128U)
	{
		printf("[FLASH_BOOT] rear64 unavailable: skip 0x08010000+ storage check\r\n");
		return;
	}

	afe_valid_a = StorageFlash_ReadSlot(FLASH_ADDR_STORAGE_AFE_SLOT_A,
										FLASH_STORAGE_MAGIC_AFE,
										(UINT16)(FLASH_STORAGE_AFE_WORD_COUNT * sizeof(UINT16)),
										0,
										&afe_seq_a);
	afe_valid_b = StorageFlash_ReadSlot(FLASH_ADDR_STORAGE_AFE_SLOT_B,
										FLASH_STORAGE_MAGIC_AFE,
										(UINT16)(FLASH_STORAGE_AFE_WORD_COUNT * sizeof(UINT16)),
										0,
										&afe_seq_b);

	rw_valid_a = StorageFlash_ReadSlot(FLASH_ADDR_STORAGE_RW_PARAM_SLOT_A,
									   FLASH_STORAGE_MAGIC_RW_PARAM,
									   (UINT16)sizeof(STORAGE_FLASH_RW_PARAM_DATA),
									   0,
									   &rw_seq_a);
	rw_valid_b = StorageFlash_ReadSlot(FLASH_ADDR_STORAGE_RW_PARAM_SLOT_B,
									   FLASH_STORAGE_MAGIC_RW_PARAM,
									   (UINT16)sizeof(STORAGE_FLASH_RW_PARAM_DATA),
									   0,
									   &rw_seq_b);

	soc_valid_a = StorageFlash_LoadJournalPage(FLASH_ADDR_STORAGE_SOC_SLOT_A,
											   FLASH_STORAGE_MAGIC_SOC,
											   (UINT16)sizeof(STORAGE_FLASH_SOC_DATA),
											   0,
											   &soc_seq_a,
											   &soc_next_a);
	soc_valid_b = StorageFlash_LoadJournalPage(FLASH_ADDR_STORAGE_SOC_SLOT_B,
											   FLASH_STORAGE_MAGIC_SOC,
											   (UINT16)sizeof(STORAGE_FLASH_SOC_DATA),
											   0,
											   &soc_seq_b,
											   &soc_next_b);
	if (!soc_valid_a)
	{
		soc_valid_a = StorageFlash_LoadJournalPage(FLASH_ADDR_STORAGE_SOC_SLOT_A,
												   FLASH_STORAGE_MAGIC_SOC,
												   (UINT16)sizeof(STORAGE_FLASH_SOC_DATA_V1),
												   0,
												   &soc_seq_a,
												   &soc_next_a);
	}
	if (!soc_valid_b)
	{
		soc_valid_b = StorageFlash_LoadJournalPage(FLASH_ADDR_STORAGE_SOC_SLOT_B,
												   FLASH_STORAGE_MAGIC_SOC,
												   (UINT16)sizeof(STORAGE_FLASH_SOC_DATA_V1),
												   0,
												   &soc_seq_b,
												   &soc_next_b);
	}

	printf("[FLASH_BOOT] AFE A=%u seq=%lu B=%u seq=%lu selected=%c\r\n",
		   afe_valid_a,
		   (unsigned long)afe_seq_a,
		   afe_valid_b,
		   (unsigned long)afe_seq_b,
		   StorageFlash_SelectLabel(afe_valid_a, afe_seq_a, afe_valid_b, afe_seq_b));
	printf("[FLASH_BOOT] RW_PARAM A=%u seq=%lu B=%u seq=%lu selected=%c\r\n",
		   rw_valid_a,
		   (unsigned long)rw_seq_a,
		   rw_valid_b,
		   (unsigned long)rw_seq_b,
		   StorageFlash_SelectLabel(rw_valid_a, rw_seq_a, rw_valid_b, rw_seq_b));
	printf("[FLASH_BOOT] SOC A=%u seq=%lu next=0x%04lX B=%u seq=%lu next=0x%04lX selected=%c\r\n",
		   soc_valid_a,
		   (unsigned long)soc_seq_a,
		   (unsigned long)(soc_next_a - FLASH_ADDR_STORAGE_SOC_SLOT_A),
		   soc_valid_b,
		   (unsigned long)soc_seq_b,
		   (unsigned long)(soc_next_b - FLASH_ADDR_STORAGE_SOC_SLOT_B),
		   StorageFlash_SelectLabel(soc_valid_a, soc_seq_a, soc_valid_b, soc_seq_b));

	update_flag = FlashReadOneHalfWord(FLASH_ADDR_UPDATE_FLAG);
	upgrade_flag = FlashReadOneHalfWord(FLASH_ADDR_UPGRADE_PARAM_FLAG);
	printf("[FLASH_BOOT] flag update=0x%04X upgrade_param=0x%04X\r\n",
		   update_flag,
		   upgrade_flag);
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
	if (AppUpgrade_RequestIap() != 0U)
	{
		__disable_fault_irq();
		MCU_RESET();
	}
}

void InitAreaSelect(void)
{
	if (AppUpgrade_IsIapRequested() != 0U)
	{
		__disable_fault_irq();
		MCU_RESET();
	}
}
