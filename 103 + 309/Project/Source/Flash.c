#include "main.h"

#define FLASH_STORAGE_MAGIC_SOC              ((UINT32)0x534F4331U)
#define FLASH_STORAGE_MAGIC_CONFIG           ((UINT32)0x43464731U)
#define FLASH_STORAGE_RECORD_VERSION         ((UINT16)0x0002U)
#define FLASH_SIZE_REG_ADDR                  ((UINT32)0x1FFFF7E0U)
#define FLASH_ERASE_RETRY_MAX                ((UINT8)3U)
#define APP_UPGRADE_MAILBOX_ADDR             ((UINT32)0x20004FE0U)
#define APP_UPGRADE_MAILBOX_MAGIC            ((UINT32)0x49415031U)
#define APP_UPGRADE_MAILBOX_REQUEST          ((UINT32)0x5AA55AA5U)

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
	UINT32 magic;
	UINT16 version;
	UINT16 length;
	UINT32 sequence;
} STORAGE_FLASH_CRC_META;

typedef struct
{
	UINT32 magic;
	UINT32 magic_inv;
	UINT32 request;
	UINT32 request_inv;
	UINT32 crc;
} APP_UPGRADE_MAILBOX;

typedef struct FLASH_RUNTIME_TAG
{
	volatile UINT8 busy;
} FLASH_RUNTIME;

static FLASH_RUNTIME s_flash;

static void StorageFlash_BeginWrite(void)
{
	s_flash.busy = 1U;
}

static void StorageFlash_EndWrite(void)
{
	s_flash.busy = 0U;
}

UINT8 StorageFlash_IsBusy(void)
{
	return s_flash.busy;
}

UINT16 StorageFlash_Crc16Update(UINT16 crc, const UINT8 *data, UINT16 length)
{
	UINT16 i;
	UINT8 bit;

	if ((data == 0) || (length == 0U))
	{
		return crc;
	}

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
				crc = (UINT16)(crc >> 1);
			}
		}
	}

	return crc;
}

UINT16 StorageFlash_Crc16(const UINT8 *data, UINT16 length)
{
	return StorageFlash_Crc16Update(0xFFFFU, data, length);
}

static UINT16 StorageFlash_CalcRecordCrc(UINT32 magic,
										 UINT16 version,
										 UINT16 length,
										 UINT32 sequence,
										 const UINT8 *payload)
{
	STORAGE_FLASH_CRC_META meta;
	UINT16 crc = 0xFFFFU;

	meta.magic = magic;
	meta.version = version;
	meta.length = length;
	meta.sequence = sequence;

	crc = StorageFlash_Crc16Update(crc, (const UINT8 *)&meta, (UINT16)sizeof(meta));
	crc = StorageFlash_Crc16Update(crc, payload, length);
	return crc;
}

static FLASH_Status FlashErasePageVerified(uint32_t page_addr)
{
	UINT32 offset;
	FLASH_Status result;

	if ((page_addr % FLASH_STORAGE_PAGE_SIZE) != 0U)
	{
		return FLASH_ERROR_PG;
	}

	result = FLASH_ErasePage(page_addr);
	if (result != FLASH_COMPLETE)
	{
		return result;
	}

	for (offset = 0U; offset < FLASH_STORAGE_PAGE_SIZE; offset += 2U)
	{
		if (FlashReadOneHalfWord(page_addr + offset) != 0xFFFFU)
		{
			return FLASH_ERROR_PG;
		}
	}

	return FLASH_COMPLETE;
}

static FLASH_Status FlashProgramHalfWordVerified(uint32_t addr, uint16_t data)
{
	FLASH_Status result;

	if ((addr & 1U) != 0U)
	{
		return FLASH_ERROR_PG;
	}

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

static FLASH_Status FlashProgramBytesVerified(uint32_t addr,
											  const UINT8 *data,
											  UINT16 length)
{
	UINT16 offset = 0U;
	UINT16 half_word;
	FLASH_Status result;

	if ((data == 0) && (length != 0U))
	{
		return FLASH_ERROR_PG;
	}

	while (offset < length)
	{
		half_word = data[offset];
		if ((offset + 1U) < length)
		{
			half_word |= ((UINT16)data[offset + 1U] << 8);
		}
		else
		{
			half_word |= 0xFF00U;
		}

		result = FlashProgramHalfWordVerified(addr + offset, half_word);
		if (result != FLASH_COMPLETE)
		{
			return result;
		}
		offset += 2U;
	}

	return FLASH_COMPLETE;
}

static UINT16 StorageFlash_RecordSpan(UINT16 payload_length)
{
	UINT32 span;

	span = (UINT32)sizeof(STORAGE_FLASH_HEADER) + payload_length;
	span = (span + (FLASH_STORAGE_RECORD_ALIGNMENT - 1U)) &
		   ~((UINT32)FLASH_STORAGE_RECORD_ALIGNMENT - 1U);
	return (UINT16)span;
}

static UINT8 StorageFlash_IsAreaBlank(uint32_t addr, UINT16 length)
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

static UINT8 StorageFlash_ReadRecord(uint32_t record_addr,
									 UINT32 expect_magic,
									 UINT16 expect_length,
									 UINT8 *payload,
									 UINT32 *sequence)
{
	STORAGE_FLASH_HEADER header;
	const UINT8 *payload_addr;
	UINT16 crc;

	if ((record_addr & ((UINT32)FLASH_STORAGE_RECORD_ALIGNMENT - 1U)) != 0U)
	{
		return 0U;
	}

	memcpy(&header, (const void *)record_addr, sizeof(header));
	payload_addr = (const UINT8 *)(record_addr + sizeof(STORAGE_FLASH_HEADER));

	if ((header.magic != expect_magic) ||
		(header.version != FLASH_STORAGE_RECORD_VERSION) ||
		(header.length != expect_length))
	{
		return 0U;
	}

	crc = StorageFlash_CalcRecordCrc(header.magic,
									 header.version,
									 header.length,
									 header.sequence,
									 payload_addr);
	if (crc != header.crc)
	{
		return 0U;
	}

	if (payload != 0)
	{
		memcpy(payload, payload_addr, expect_length);
	}
	if (sequence != 0)
	{
		*sequence = header.sequence;
	}
	return 1U;
}

static FLASH_Status StorageFlash_ProgramRecord(uint32_t record_addr,
											   UINT32 magic,
											   const UINT8 *payload,
											   UINT16 length,
											   UINT32 sequence)
{
	STORAGE_FLASH_HEADER header;
	FLASH_Status result;

	if ((payload == 0) ||
		((record_addr & ((UINT32)FLASH_STORAGE_RECORD_ALIGNMENT - 1U)) != 0U))
	{
		return FLASH_ERROR_PG;
	}

	header.magic = magic;
	header.version = FLASH_STORAGE_RECORD_VERSION;
	header.length = length;
	header.sequence = sequence;
	header.crc = StorageFlash_CalcRecordCrc(magic,
											 FLASH_STORAGE_RECORD_VERSION,
											 length,
											 sequence,
											 payload);
	header.reserved = 0xFFFFU;

	result = FlashProgramBytesVerified(record_addr,
									   (const UINT8 *)&header,
									   (UINT16)sizeof(header));
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
	UINT8 valid_a;
	UINT8 valid_b;
	UINT32 seq_a = 0U;
	UINT32 seq_b = 0U;
	uint32_t chosen;

	if (payload == 0)
	{
		return 0U;
	}

	valid_a = StorageFlash_ReadRecord(slot_a, expect_magic, expect_length, 0, &seq_a);
	valid_b = StorageFlash_ReadRecord(slot_b, expect_magic, expect_length, 0, &seq_b);
	if (!valid_a && !valid_b)
	{
		return 0U;
	}

	if (valid_a && valid_b)
	{
		chosen = (seq_a >= seq_b) ? slot_a : slot_b;
	}
	else
	{
		chosen = valid_a ? slot_a : slot_b;
	}

	return StorageFlash_ReadRecord(chosen, expect_magic, expect_length, payload, 0);
}

static FLASH_Status StorageFlash_WritePairSlot(uint32_t slot_addr,
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
	UINT8 valid_a;
	UINT8 valid_b;
	UINT32 seq_a = 0U;
	UINT32 seq_b = 0U;
	UINT32 next_sequence = 1U;
	UINT32 verify_sequence = 0U;
	uint32_t target_slot = slot_a;
	FLASH_Status result;

	if ((payload == 0) ||
		((UINT32)sizeof(STORAGE_FLASH_HEADER) + length > FLASH_STORAGE_PAGE_SIZE))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}

	valid_a = StorageFlash_ReadRecord(slot_a, magic, length, 0, &seq_a);
	valid_b = StorageFlash_ReadRecord(slot_b, magic, length, 0, &seq_b);

	if (valid_a && valid_b)
	{
		if (seq_a >= seq_b)
		{
			next_sequence = seq_a + 1U;
			target_slot = slot_b;
		}
		else
		{
			next_sequence = seq_b + 1U;
			target_slot = slot_a;
		}
	}
	else if (valid_a)
	{
		next_sequence = seq_a + 1U;
		target_slot = slot_b;
	}
	else if (valid_b)
	{
		next_sequence = seq_b + 1U;
		target_slot = slot_a;
	}

	result = StorageFlash_WritePairSlot(target_slot,
											magic,
											payload,
											length,
											next_sequence);
	if ((result != FLASH_COMPLETE) ||
		(!StorageFlash_ReadRecord(target_slot, magic, length, 0, &verify_sequence)) ||
		(verify_sequence != next_sequence) ||
		(memcmp((const void *)(target_slot + sizeof(STORAGE_FLASH_HEADER)),
				payload,
				length) != 0))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}

	return 1U;
}

static UINT8 StorageFlash_LoadJournalPage(uint32_t slot_addr,
										  UINT32 expect_magic,
										  UINT16 expect_length,
										  UINT8 *payload,
										  UINT32 *sequence,
										  UINT32 *next_addr)
{
	UINT16 record_span;
	UINT32 offset;
	UINT32 record_addr;
	UINT32 latest_addr = 0U;
	UINT32 latest_sequence = 0U;
	UINT32 current_sequence = 0U;
	UINT32 blank_addr;
	UINT8 found = 0U;

	record_span = StorageFlash_RecordSpan(expect_length);
	blank_addr = slot_addr + FLASH_STORAGE_PAGE_SIZE;
	if ((record_span == 0U) || (record_span > FLASH_STORAGE_PAGE_SIZE))
	{
		return 0U;
	}

	for (offset = 0U;
		 (offset + record_span) <= FLASH_STORAGE_PAGE_SIZE;
		 offset += record_span)
	{
		record_addr = slot_addr + offset;
		if (StorageFlash_IsAreaBlank(record_addr, record_span))
		{
			blank_addr = record_addr;
			break;
		}

		if (StorageFlash_ReadRecord(record_addr,
									 expect_magic,
									 expect_length,
									 0,
									 &current_sequence) &&
			((!found) || (current_sequence >= latest_sequence)))
		{
			latest_sequence = current_sequence;
			latest_addr = record_addr;
			found = 1U;
		}
	}

	if (found && (payload != 0))
	{
		memcpy(payload,
			   (const void *)(latest_addr + sizeof(STORAGE_FLASH_HEADER)),
			   expect_length);
	}
	if (sequence != 0)
	{
		*sequence = found ? latest_sequence : 0U;
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
	UINT8 valid_a;
	UINT8 valid_b;
	UINT32 seq_a = 0U;
	UINT32 seq_b = 0U;
	uint32_t chosen;

	if (payload == 0)
	{
		return 0U;
	}

	valid_a = StorageFlash_LoadJournalPage(slot_a, expect_magic, expect_length, 0, &seq_a, 0);
	valid_b = StorageFlash_LoadJournalPage(slot_b, expect_magic, expect_length, 0, &seq_b, 0);
	if (!valid_a && !valid_b)
	{
		return 0U;
	}

	if (valid_a && valid_b)
	{
		chosen = (seq_a >= seq_b) ? slot_a : slot_b;
	}
	else
	{
		chosen = valid_a ? slot_a : slot_b;
	}

	return StorageFlash_LoadJournalPage(chosen, expect_magic, expect_length, payload, 0, 0);
}

static UINT8 StorageFlash_SaveJournalPair(uint32_t slot_a,
										  uint32_t slot_b,
										  UINT32 magic,
										  const UINT8 *payload,
										  UINT16 length)
{
	UINT16 record_span;
	UINT8 valid_a;
	UINT8 valid_b;
	UINT8 erase_target_page = 0U;
	UINT32 seq_a = 0U;
	UINT32 seq_b = 0U;
	UINT32 next_addr_a = slot_a;
	UINT32 next_addr_b = slot_b;
	UINT32 next_sequence = 1U;
	UINT32 target_page = slot_a;
	UINT32 target_addr = slot_a;
	UINT32 verify_sequence = 0U;
	FLASH_Status result;

	record_span = StorageFlash_RecordSpan(length);
	if ((payload == 0) || (record_span == 0U) || (record_span > FLASH_STORAGE_PAGE_SIZE))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}

	valid_a = StorageFlash_LoadJournalPage(slot_a, magic, length, 0, &seq_a, &next_addr_a);
	valid_b = StorageFlash_LoadJournalPage(slot_b, magic, length, 0, &seq_b, &next_addr_b);

	if (valid_a && valid_b)
	{
		if (seq_a >= seq_b)
		{
			next_sequence = seq_a + 1U;
			target_page = slot_a;
			target_addr = next_addr_a;
		}
		else
		{
			next_sequence = seq_b + 1U;
			target_page = slot_b;
			target_addr = next_addr_b;
		}
	}
	else if (valid_a)
	{
		next_sequence = seq_a + 1U;
		target_page = slot_a;
		target_addr = next_addr_a;
	}
	else if (valid_b)
	{
		next_sequence = seq_b + 1U;
		target_page = slot_b;
		target_addr = next_addr_b;
	}
	else
	{
		erase_target_page = StorageFlash_IsAreaBlank(target_page,
														   (UINT16)FLASH_STORAGE_PAGE_SIZE) ? 0U : 1U;
	}

	if ((target_addr + record_span) > (target_page + FLASH_STORAGE_PAGE_SIZE))
	{
		target_page = (target_page == slot_a) ? slot_b : slot_a;
		target_addr = target_page;
		erase_target_page = 1U;
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
			return 0U;
		}
	}

	result = StorageFlash_ProgramRecord(target_addr, magic, payload, length, next_sequence);
	FLASH_Lock();
	if ((result != FLASH_COMPLETE) ||
		(!StorageFlash_ReadRecord(target_addr, magic, length, 0, &verify_sequence)) ||
		(verify_sequence != next_sequence) ||
		(memcmp((const void *)(target_addr + sizeof(STORAGE_FLASH_HEADER)), payload, length) != 0))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}

	return 1U;
}

FLASH_Status FlashWriteOneHalfWord(uint32_t StartAddr, uint16_t Buffer)
{
	FLASH_Status result = FLASH_ERROR_PG;
	UINT8 retry;

	StorageFlash_BeginWrite();
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	for (retry = 0U; retry < FLASH_ERASE_RETRY_MAX; ++retry)
	{
		result = FlashErasePageVerified(StartAddr);
		if (result == FLASH_COMPLETE)
		{
			break;
		}
		FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	}
	if (result == FLASH_COMPLETE)
	{
		result = FlashProgramHalfWordVerified(StartAddr, Buffer);
	}
	FLASH_Lock();
	StorageFlash_EndWrite();
	return result;
}

UINT16 FlashReadOneHalfWord(UINT32 faddr)
{
	return *(vu16 *)faddr;
}

static UINT8 StorageFlash_RangeIsValid(UINT32 addr, UINT16 length)
{
	UINT32 end_addr;

	if ((length == 0U) || ((addr & 1U) != 0U))
	{
		return 0U;
	}
	end_addr = addr + (UINT32)length;
	if ((end_addr < addr) ||
		(addr < FLASH_ADDR_STORAGE_START) ||
		(end_addr > FLASH_ADDR_STORAGE_END))
	{
		return 0U;
	}
	return 1U;
}

UINT8 StorageFlash_EraseStoragePage(UINT32 page_addr)
{
	FLASH_Status result;

	if ((page_addr < FLASH_ADDR_STORAGE_START) ||
		(page_addr >= FLASH_ADDR_STORAGE_END) ||
		((page_addr % FLASH_STORAGE_PAGE_SIZE) != 0U))
	{
		return 0U;
	}

	StorageFlash_BeginWrite();
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	result = FlashErasePageVerified(page_addr);
	FLASH_Lock();
	StorageFlash_EndWrite();

	if (result != FLASH_COMPLETE)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}
	return 1U;
}

UINT8 StorageFlash_ProgramStorageBytes(UINT32 addr, const UINT8 *data, UINT16 length)
{
	FLASH_Status result;

	if ((data == 0) || !StorageFlash_RangeIsValid(addr, length))
	{
		return 0U;
	}

	StorageFlash_BeginWrite();
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	result = FlashProgramBytesVerified(addr, data, length);
	FLASH_Lock();
	StorageFlash_EndWrite();

	if (result != FLASH_COMPLETE)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}
	return 1U;
}

static volatile APP_UPGRADE_MAILBOX *AppUpgrade_Mailbox(void)
{
	return (volatile APP_UPGRADE_MAILBOX *)APP_UPGRADE_MAILBOX_ADDR;
}

static UINT32 AppUpgrade_MailboxCrc(UINT32 magic, UINT32 request)
{
	return magic ^ request ^ 0xA5A55A5AU;
}

static UINT8 AppUpgrade_IsIapRequested(void)
{
	volatile APP_UPGRADE_MAILBOX *mailbox = AppUpgrade_Mailbox();

	if ((mailbox->magic != APP_UPGRADE_MAILBOX_MAGIC) ||
		(mailbox->magic_inv != (UINT32)~APP_UPGRADE_MAILBOX_MAGIC) ||
		(mailbox->request != APP_UPGRADE_MAILBOX_REQUEST) ||
		(mailbox->request_inv != (UINT32)~APP_UPGRADE_MAILBOX_REQUEST) ||
		(mailbox->crc != AppUpgrade_MailboxCrc(APP_UPGRADE_MAILBOX_MAGIC,
															  APP_UPGRADE_MAILBOX_REQUEST)))
	{
		return 0U;
	}
	return 1U;
}

UINT8 AppUpgrade_RequestIap(void)
{
	volatile APP_UPGRADE_MAILBOX *mailbox = AppUpgrade_Mailbox();

	mailbox->magic = APP_UPGRADE_MAILBOX_MAGIC;
	mailbox->magic_inv = (UINT32)~APP_UPGRADE_MAILBOX_MAGIC;
	mailbox->request = APP_UPGRADE_MAILBOX_REQUEST;
	mailbox->request_inv = (UINT32)~APP_UPGRADE_MAILBOX_REQUEST;
	mailbox->crc = AppUpgrade_MailboxCrc(APP_UPGRADE_MAILBOX_MAGIC,
														 APP_UPGRADE_MAILBOX_REQUEST);
	return AppUpgrade_IsIapRequested();
}

UINT8 StorageFlash_LoadConfigData(BMS_CONFIG *data)
{
	if (data == 0)
	{
		return 0U;
	}

	if (!StorageFlash_LoadPair(FLASH_ADDR_STORAGE_CONFIG_SLOT_A,
									 FLASH_ADDR_STORAGE_CONFIG_SLOT_B,
									 FLASH_STORAGE_MAGIC_CONFIG,
									 (UINT16)sizeof(*data),
									 (UINT8 *)data))
	{
		return 0U;
	}

	return (data->u16FormatVersion == FLASH_STORAGE_CONFIG_FORMAT_VERSION) ? 1U : 0U;
}

UINT8 StorageFlash_SaveConfigData(const BMS_CONFIG *data)
{
	BMS_CONFIG save_data;
	UINT8 result;

	if (data == 0)
	{
		return 0U;
	}

	save_data = *data;
	save_data.u16FormatVersion = FLASH_STORAGE_CONFIG_FORMAT_VERSION;
	StorageFlash_BeginWrite();
	result = StorageFlash_SavePair(FLASH_ADDR_STORAGE_CONFIG_SLOT_A,
									  FLASH_ADDR_STORAGE_CONFIG_SLOT_B,
									  FLASH_STORAGE_MAGIC_CONFIG,
									  (const UINT8 *)&save_data,
									  (UINT16)sizeof(save_data));
	StorageFlash_EndWrite();
	return result;
}

UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data)
{
	if (data == 0)
	{
		return 0U;
	}

	if (!StorageFlash_LoadJournalPair(FLASH_ADDR_STORAGE_SOC_SLOT_A,
									   FLASH_ADDR_STORAGE_SOC_SLOT_B,
									   FLASH_STORAGE_MAGIC_SOC,
									   (UINT16)sizeof(STORAGE_FLASH_SOC_DATA),
									   (UINT8 *)data))
	{
		return 0U;
	}

	return (data->u16FormatVersion == FLASH_STORAGE_SOC_DATA_VERSION_CURRENT) ? 1U : 0U;
}

UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data)
{
	STORAGE_FLASH_SOC_DATA save_data;
	UINT8 result;

	if (data == 0)
	{
		return 0U;
	}

	save_data = *data;
	save_data.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_CURRENT;
	StorageFlash_BeginWrite();
	result = StorageFlash_SaveJournalPair(FLASH_ADDR_STORAGE_SOC_SLOT_A,
										  FLASH_ADDR_STORAGE_SOC_SLOT_B,
										  FLASH_STORAGE_MAGIC_SOC,
										  (const UINT8 *)&save_data,
										  (UINT16)sizeof(save_data));
	StorageFlash_EndWrite();
	return result;
}

void StorageFlash_PrintBootCheck(void)
{
	UINT16 flash_size_kb = *((volatile UINT16 *)FLASH_SIZE_REG_ADDR);
	BMS_CONFIG config;
	UINT8 config_valid;
	UINT16 policy_version;

	printf("\r\n[FLASH_BOOT] flash_size_reg=%uKB page=%lu align=%u storage=%luKB app_max=%luKB\r\n",
		   flash_size_kb,
		   (unsigned long)FLASH_STORAGE_PAGE_SIZE,
		   FLASH_STORAGE_RECORD_ALIGNMENT,
		   (unsigned long)((FLASH_ADDR_STORAGE_END - FLASH_ADDR_STORAGE_START) / 1024U),
		   (unsigned long)(FLASH_APP_MAX_SIZE / 1024U));
	if (flash_size_kb < 128U)
	{
		printf("[FLASH_BOOT] rear64 unavailable: skip 0x08010000+ storage check\r\n");
		return;
	}

	config_valid = StorageFlash_LoadConfigData(&config);
	policy_version = config_valid ? config.u16AppliedPolicyVersion : FLASH_UPGRADE_PARAM_FLAG_RESET;
	printf("[FLASH_BOOT] config=%u policy=0x%04X iap_mailbox=%u\r\n",
		   config_valid,
		   policy_version,
		   AppUpgrade_IsIapRequested());
}

void App_FlashUpdate(void)
{
	if (1 == u8FlashUpdateFlag)
	{
		SH367309_DriverMos_Ctrl(GPIO_CHG, 0);
		SH367309_DriverMos_Ctrl(GPIO_DSG, 0);
		__delay_ms(10);
		u8FlashUpdateFlag = 0;
		__disable_fault_irq();
		MCU_RESET();
	}
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
