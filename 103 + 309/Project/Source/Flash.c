#include "main.h"

#define FLASH_STORAGE_MAGIC_SOC             ((UINT32)0x534F4331U)
#define FLASH_STORAGE_MAGIC_AFE             ((UINT32)0x41464531U)
#define FLASH_STORAGE_MAGIC_RW_PARAM        ((UINT32)0x52575031U)
#define FLASH_STORAGE_MAGIC_CONFIG          ((UINT32)0x43464731U)
#define FLASH_STORAGE_MAGIC_LOG             ((UINT32)0x4C4F4731U)
#define FLASH_STORAGE_MAGIC_FACTORY_AGING   ((UINT32)0x41474531U)
#define FLASH_STORAGE_RECORD_VERSION_LEGACY ((UINT16)0x0001U)
#define FLASH_STORAGE_RECORD_VERSION        ((UINT16)0x0002U)
#define FLASH_SIZE_REG_ADDR                 ((UINT32)0x1FFFF7E0U)
#define FLASH_ERASE_RETRY_MAX               ((UINT8)3U)
#define APP_UPGRADE_MAILBOX_ADDR            ((UINT32)0x20004FE0U)
#define APP_UPGRADE_MAILBOX_MAGIC           ((UINT32)0x49415031U)
#define APP_UPGRADE_MAILBOX_REQUEST         ((UINT32)0x5AA55AA5U)

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
	UINT32 magic;
	UINT16 version;
	UINT16 length;
	UINT32 sequence;
} STORAGE_FLASH_CRC_META;

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

typedef struct FLASH_RUNTIME_TAG
{
	volatile UINT8 busy;
} FLASH_RUNTIME;

static FLASH_RUNTIME s_flash;

static void StorageFlash_BeginWrite(void) { s_flash.busy = 1U; }
static void StorageFlash_EndWrite(void) { s_flash.busy = 0U; }
UINT8 StorageFlash_IsBusy(void) { return s_flash.busy; }

static UINT16 StorageFlash_CrcUpdate(UINT16 crc, const UINT8 *data, UINT16 length)
{
	UINT16 i;
	UINT8 bit;
	if ((data == 0) || (length == 0U)) return crc;
	for (i = 0U; i < length; ++i)
	{
		crc ^= data[i];
		for (bit = 0U; bit < 8U; ++bit)
		{
			crc = (crc & 1U) ? (UINT16)((crc >> 1) ^ 0xA001U) : (UINT16)(crc >> 1);
		}
	}
	return crc;
}

static UINT16 StorageFlash_CalcLegacyPayloadCrc(const UINT8 *payload, UINT16 length)
{
	return StorageFlash_CrcUpdate(0xFFFFU, payload, length);
}

static UINT16 StorageFlash_CalcRecordCrc(UINT32 magic, UINT16 version, UINT16 length,
										 UINT32 sequence, const UINT8 *payload)
{
	STORAGE_FLASH_CRC_META meta;
	UINT16 crc = 0xFFFFU;
	meta.magic = magic;
	meta.version = version;
	meta.length = length;
	meta.sequence = sequence;
	crc = StorageFlash_CrcUpdate(crc, (const UINT8 *)&meta, (UINT16)sizeof(meta));
	return StorageFlash_CrcUpdate(crc, payload, length);
}

static FLASH_Status FlashErasePageVerified(uint32_t page_addr)
{
	UINT32 offset;
	FLASH_Status result;
	if ((page_addr % FLASH_STORAGE_PAGE_SIZE) != 0U) return FLASH_ERROR_PG;
	result = FLASH_ErasePage(page_addr);
	if (result != FLASH_COMPLETE) return result;
	for (offset = 0U; offset < FLASH_STORAGE_PAGE_SIZE; offset += 2U)
	{
		if (FlashReadOneHalfWord(page_addr + offset) != 0xFFFFU) return FLASH_ERROR_PG;
	}
	return FLASH_COMPLETE;
}

static FLASH_Status FlashProgramHalfWordVerified(uint32_t addr, uint16_t data)
{
	FLASH_Status result;
	if ((addr & 1U) != 0U) return FLASH_ERROR_PG;
	result = FLASH_ProgramHalfWord(addr, data);
	if (result != FLASH_COMPLETE) return result;
	return (FlashReadOneHalfWord(addr) == data) ? FLASH_COMPLETE : FLASH_ERROR_PG;
}

static FLASH_Status FlashProgramBytesVerified(uint32_t addr, const UINT8 *data, UINT16 length)
{
	UINT16 offset = 0U;
	UINT16 half_word;
	FLASH_Status result;
	if ((data == 0) && (length != 0U)) return FLASH_ERROR_PG;
	while (offset < length)
	{
		half_word = data[offset];
		if ((offset + 1U) < length) half_word |= ((UINT16)data[offset + 1U] << 8);
		else half_word |= 0xFF00U;
		result = FlashProgramHalfWordVerified(addr + offset, half_word);
		if (result != FLASH_COMPLETE) return result;
		offset += 2U;
	}
	return FLASH_COMPLETE;
}

static UINT16 StorageFlash_RecordSpan(UINT16 payload_length)
{
	UINT32 span = (UINT32)sizeof(STORAGE_FLASH_HEADER) + payload_length;
	span = (span + (FLASH_STORAGE_RECORD_ALIGNMENT - 1U)) & ~((UINT32)FLASH_STORAGE_RECORD_ALIGNMENT - 1U);
	return (UINT16)span;
}

static UINT8 StorageFlash_IsAreaBlank(uint32_t addr, UINT16 length)
{
	UINT16 offset;
	for (offset = 0U; offset < length; offset += 2U)
	{
		if (FlashReadOneHalfWord(addr + offset) != 0xFFFFU) return 0U;
	}
	return 1U;
}

static UINT8 StorageFlash_ReadRecord(uint32_t record_addr, UINT32 expect_magic,
									 UINT16 expect_length, UINT8 *payload, UINT32 *sequence)
{
	STORAGE_FLASH_HEADER header;
	const UINT8 *payload_addr = (const UINT8 *)(record_addr + sizeof(STORAGE_FLASH_HEADER));
	UINT16 crc;

	if ((record_addr & ((UINT32)FLASH_STORAGE_RECORD_ALIGNMENT - 1U)) != 0U)
	{
		return 0U;
	}

	memcpy(&header, (const void *)record_addr, sizeof(header));
	if ((header.magic != expect_magic) || (header.length != expect_length) ||
		((header.version != FLASH_STORAGE_RECORD_VERSION) &&
		 (header.version != FLASH_STORAGE_RECORD_VERSION_LEGACY))) return 0U;
	if (header.version == FLASH_STORAGE_RECORD_VERSION)
		crc = StorageFlash_CalcRecordCrc(header.magic, header.version, header.length,
										 header.sequence, payload_addr);
	else
		crc = StorageFlash_CalcLegacyPayloadCrc(payload_addr, expect_length);
	if (crc != header.crc) return 0U;
	if (payload != 0) memcpy(payload, payload_addr, expect_length);
	if (sequence != 0) *sequence = header.sequence;
	return 1U;
}

static FLASH_Status StorageFlash_ProgramRecord(uint32_t record_addr, UINT32 magic,
											   const UINT8 *payload, UINT16 length, UINT32 sequence)
{
	STORAGE_FLASH_HEADER header;
	FLASH_Status result;
	if ((payload == 0) || ((record_addr & ((UINT32)FLASH_STORAGE_RECORD_ALIGNMENT - 1U)) != 0U))
		return FLASH_ERROR_PG;
	header.magic = magic;
	header.version = FLASH_STORAGE_RECORD_VERSION;
	header.length = length;
	header.sequence = sequence;
	header.crc = StorageFlash_CalcRecordCrc(magic, FLASH_STORAGE_RECORD_VERSION, length, sequence, payload);
	header.reserved = 0xFFFFU;
	result = FlashProgramBytesVerified(record_addr, (const UINT8 *)&header, (UINT16)sizeof(header));
	if (result == FLASH_COMPLETE)
		result = FlashProgramBytesVerified(record_addr + sizeof(header), payload, length);
	return result;
}

static UINT8 StorageFlash_LoadPair(uint32_t slot_a, uint32_t slot_b, UINT32 expect_magic,
								   UINT16 expect_length, UINT8 *payload)
{
	UINT8 valid_a, valid_b;
	UINT32 seq_a = 0U, seq_b = 0U;
	uint32_t chosen;
	if (payload == 0) return 0U;
	valid_a = StorageFlash_ReadRecord(slot_a, expect_magic, expect_length, 0, &seq_a);
	valid_b = StorageFlash_ReadRecord(slot_b, expect_magic, expect_length, 0, &seq_b);
	if (!valid_a && !valid_b) return 0U;
	if (valid_a && valid_b) chosen = (seq_a >= seq_b) ? slot_a : slot_b;
	else chosen = valid_a ? slot_a : slot_b;
	return StorageFlash_ReadRecord(chosen, expect_magic, expect_length, payload, 0);
}

static FLASH_Status StorageFlash_WritePairSlot(uint32_t slot_addr, UINT32 magic,
											 const UINT8 *payload, UINT16 length, UINT32 sequence)
{
	FLASH_Status result;
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	result = FlashErasePageVerified(slot_addr);
	if (result == FLASH_COMPLETE) result = StorageFlash_ProgramRecord(slot_addr, magic, payload, length, sequence);
	FLASH_Lock();
	return result;
}

static UINT8 StorageFlash_SavePair(uint32_t slot_a, uint32_t slot_b, UINT32 magic,
								   const UINT8 *payload, UINT16 length)
{
	UINT8 valid_a, valid_b;
	UINT32 seq_a = 0U, seq_b = 0U, next_sequence = 1U, verify_sequence = 0U;
	uint32_t target_slot = slot_a;
	FLASH_Status result;
	if ((payload == 0) || ((UINT32)sizeof(STORAGE_FLASH_HEADER) + length > FLASH_STORAGE_PAGE_SIZE))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}
	valid_a = StorageFlash_ReadRecord(slot_a, magic, length, 0, &seq_a);
	valid_b = StorageFlash_ReadRecord(slot_b, magic, length, 0, &seq_b);
	if (valid_a && valid_b)
	{
		if (seq_a >= seq_b) { next_sequence = seq_a + 1U; target_slot = slot_b; }
		else { next_sequence = seq_b + 1U; target_slot = slot_a; }
	}
	else if (valid_a) { next_sequence = seq_a + 1U; target_slot = slot_b; }
	else if (valid_b) { next_sequence = seq_b + 1U; target_slot = slot_a; }
	result = StorageFlash_WritePairSlot(target_slot, magic, payload, length, next_sequence);
	if (result != FLASH_COMPLETE ||
		!StorageFlash_ReadRecord(target_slot, magic, length, 0, &verify_sequence) ||
		verify_sequence != next_sequence ||
		memcmp((const void *)(target_slot + sizeof(STORAGE_FLASH_HEADER)), payload, length) != 0)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}
	return 1U;
}

static UINT8 StorageFlash_LoadJournalPage(uint32_t slot_addr, UINT32 expect_magic,
										  UINT16 expect_length, UINT8 *payload,
										  UINT32 *sequence, UINT32 *next_addr)
{
	UINT16 record_span = StorageFlash_RecordSpan(expect_length);
	UINT32 offset, record_addr, latest_addr = 0U, latest_sequence = 0U, current_sequence = 0U;
	UINT32 blank_addr = slot_addr + FLASH_STORAGE_PAGE_SIZE;
	UINT8 found = 0U;
	if ((record_span == 0U) || (record_span > FLASH_STORAGE_PAGE_SIZE)) return 0U;
	for (offset = 0U; (offset + record_span) <= FLASH_STORAGE_PAGE_SIZE; offset += record_span)
	{
		record_addr = slot_addr + offset;
		if (StorageFlash_IsAreaBlank(record_addr, record_span)) { blank_addr = record_addr; break; }
		if (StorageFlash_ReadRecord(record_addr, expect_magic, expect_length, 0, &current_sequence) &&
			((!found) || (current_sequence >= latest_sequence)))
		{
			latest_sequence = current_sequence;
			latest_addr = record_addr;
			found = 1U;
		}
	}
	if (found && (payload != 0)) memcpy(payload, (const void *)(latest_addr + sizeof(STORAGE_FLASH_HEADER)), expect_length);
	if (sequence != 0) *sequence = found ? latest_sequence : 0U;
	if (next_addr != 0) *next_addr = blank_addr;
	return found;
}

static UINT8 StorageFlash_LoadJournalPair(uint32_t slot_a, uint32_t slot_b, UINT32 expect_magic,
										  UINT16 expect_length, UINT8 *payload)
{
	UINT8 valid_a, valid_b;
	UINT32 seq_a = 0U, seq_b = 0U;
	uint32_t chosen;
	if (payload == 0) return 0U;
	valid_a = StorageFlash_LoadJournalPage(slot_a, expect_magic, expect_length, 0, &seq_a, 0);
	valid_b = StorageFlash_LoadJournalPage(slot_b, expect_magic, expect_length, 0, &seq_b, 0);
	if (!valid_a && !valid_b) return 0U;
	if (valid_a && valid_b) chosen = (seq_a >= seq_b) ? slot_a : slot_b;
	else chosen = valid_a ? slot_a : slot_b;
	return StorageFlash_LoadJournalPage(chosen, expect_magic, expect_length, payload, 0, 0);
}

static UINT8 StorageFlash_SaveJournalPair(uint32_t slot_a, uint32_t slot_b, UINT32 magic,
										  const UINT8 *payload, UINT16 length)
{
	UINT16 record_span = StorageFlash_RecordSpan(length);
	UINT8 valid_a, valid_b, erase_target_page = 0U;
	UINT32 seq_a = 0U, seq_b = 0U, next_addr_a = slot_a, next_addr_b = slot_b;
	UINT32 next_sequence = 1U, target_page = slot_a, target_addr = slot_a, verify_sequence = 0U;
	FLASH_Status result;
	if ((payload == 0) || (record_span == 0U) || (record_span > FLASH_STORAGE_PAGE_SIZE))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}
	valid_a = StorageFlash_LoadJournalPage(slot_a, magic, length, 0, &seq_a, &next_addr_a);
	valid_b = StorageFlash_LoadJournalPage(slot_b, magic, length, 0, &seq_b, &next_addr_b);
	if (valid_a && valid_b)
	{
		if (seq_a >= seq_b) { next_sequence = seq_a + 1U; target_page = slot_a; target_addr = next_addr_a; }
		else { next_sequence = seq_b + 1U; target_page = slot_b; target_addr = next_addr_b; }
	}
	else if (valid_a) { next_sequence = seq_a + 1U; target_page = slot_a; target_addr = next_addr_a; }
	else if (valid_b) { next_sequence = seq_b + 1U; target_page = slot_b; target_addr = next_addr_b; }
	else erase_target_page = StorageFlash_IsAreaBlank(target_page, (UINT16)FLASH_STORAGE_PAGE_SIZE) ? 0U : 1U;
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
		if (result != FLASH_COMPLETE) { FLASH_Lock(); System_ERROR_UserCallback(ERROR_EEPROM_STORE); return 0U; }
	}
	result = StorageFlash_ProgramRecord(target_addr, magic, payload, length, next_sequence);
	FLASH_Lock();
	if (result != FLASH_COMPLETE ||
		!StorageFlash_ReadRecord(target_addr, magic, length, 0, &verify_sequence) ||
		verify_sequence != next_sequence ||
		memcmp((const void *)(target_addr + sizeof(STORAGE_FLASH_HEADER)), payload, length) != 0)
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
		if (result == FLASH_COMPLETE) break;
		FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	}
	if (result == FLASH_COMPLETE) result = FlashProgramHalfWordVerified(StartAddr, Buffer);
	FLASH_Lock();
	StorageFlash_EndWrite();
	return result;
}

UINT16 FlashReadOneHalfWord(UINT32 faddr) { return *(vu16 *)faddr; }

static volatile APP_UPGRADE_MAILBOX *AppUpgrade_Mailbox(void)
{
	return (volatile APP_UPGRADE_MAILBOX *)APP_UPGRADE_MAILBOX_ADDR;
}
static UINT32 AppUpgrade_MailboxCrc(UINT32 magic, UINT32 request) { return magic ^ request ^ 0xA5A55A5AU; }
static UINT8 AppUpgrade_IsIapRequested(void)
{
	volatile APP_UPGRADE_MAILBOX *mailbox = AppUpgrade_Mailbox();
	return ((mailbox->magic == APP_UPGRADE_MAILBOX_MAGIC) &&
		(mailbox->magic_inv == (UINT32)~APP_UPGRADE_MAILBOX_MAGIC) &&
		(mailbox->request == APP_UPGRADE_MAILBOX_REQUEST) &&
		(mailbox->request_inv == (UINT32)~APP_UPGRADE_MAILBOX_REQUEST) &&
		(mailbox->crc == AppUpgrade_MailboxCrc(APP_UPGRADE_MAILBOX_MAGIC, APP_UPGRADE_MAILBOX_REQUEST))) ? 1U : 0U;
}
UINT8 AppUpgrade_RequestIap(void)
{
	volatile APP_UPGRADE_MAILBOX *mailbox = AppUpgrade_Mailbox();
	mailbox->magic = APP_UPGRADE_MAILBOX_MAGIC;
	mailbox->magic_inv = (UINT32)~APP_UPGRADE_MAILBOX_MAGIC;
	mailbox->request = APP_UPGRADE_MAILBOX_REQUEST;
	mailbox->request_inv = (UINT32)~APP_UPGRADE_MAILBOX_REQUEST;
	mailbox->crc = AppUpgrade_MailboxCrc(APP_UPGRADE_MAILBOX_MAGIC, APP_UPGRADE_MAILBOX_REQUEST);
	return AppUpgrade_IsIapRequested();
}

static UINT8 StorageFlash_LoadConfigData(STORAGE_FLASH_CONFIG_DATA *data)
{
	if (data == 0) return 0U;
	if (!StorageFlash_LoadPair(FLASH_ADDR_STORAGE_CONFIG_SLOT_A, FLASH_ADDR_STORAGE_CONFIG_SLOT_B,
								 FLASH_STORAGE_MAGIC_CONFIG, (UINT16)sizeof(*data), (UINT8 *)data)) return 0U;
	return (data->u16FormatVersion == FLASH_STORAGE_CONFIG_FORMAT_VERSION) ? 1U : 0U;
}

static UINT8 StorageFlash_SaveConfigData(const STORAGE_FLASH_CONFIG_DATA *data)
{
	STORAGE_FLASH_CONFIG_DATA save_data;
	UINT8 result;
	if (data == 0) return 0U;
	save_data = *data;
	save_data.u16FormatVersion = FLASH_STORAGE_CONFIG_FORMAT_VERSION;
	StorageFlash_BeginWrite();
	result = StorageFlash_SavePair(FLASH_ADDR_STORAGE_CONFIG_SLOT_A, FLASH_ADDR_STORAGE_CONFIG_SLOT_B,
								   FLASH_STORAGE_MAGIC_CONFIG, (const UINT8 *)&save_data, (UINT16)sizeof(save_data));
	StorageFlash_EndWrite();
	return result;
}

static UINT8 StorageFlash_LoadLegacyAfe(UINT16 *values)
{
	return StorageFlash_LoadPair(FLASH_ADDR_STORAGE_AFE_SLOT_A, FLASH_ADDR_STORAGE_AFE_SLOT_B,
								 FLASH_STORAGE_MAGIC_AFE,
								 (UINT16)(FLASH_STORAGE_AFE_WORD_COUNT * sizeof(UINT16)), (UINT8 *)values);
}

static UINT8 StorageFlash_LoadLegacyRw(STORAGE_FLASH_RW_PARAM_DATA *data)
{
	return StorageFlash_LoadPair(FLASH_ADDR_STORAGE_RW_PARAM_SLOT_A, FLASH_ADDR_STORAGE_RW_PARAM_SLOT_B,
								 FLASH_STORAGE_MAGIC_RW_PARAM, (UINT16)sizeof(*data), (UINT8 *)data);
}

static void StorageFlash_ConfigFromLegacy(STORAGE_FLASH_CONFIG_DATA *config,
										  const UINT16 *afe, const STORAGE_FLASH_RW_PARAM_DATA *rw)
{
	memset(config, 0xFF, sizeof(*config));
	config->u16FormatVersion = FLASH_STORAGE_CONFIG_FORMAT_VERSION;
	config->u16AppliedPolicyVersion = FlashReadOneHalfWord(FLASH_ADDR_UPGRADE_PARAM_FLAG);
	memcpy(config->afe, afe, sizeof(config->afe));
	memcpy(config->protect, rw->protect, sizeof(config->protect));
	memcpy(config->other, rw->other, sizeof(config->other));
	memcpy(config->reserved, rw->reserved, sizeof(config->reserved));
}

static UINT8 StorageFlash_TryMigrateLegacyConfig(STORAGE_FLASH_CONFIG_DATA *config)
{
	UINT16 afe[FLASH_STORAGE_AFE_WORD_COUNT];
	STORAGE_FLASH_RW_PARAM_DATA rw;
	if (!StorageFlash_LoadLegacyAfe(afe) || !StorageFlash_LoadLegacyRw(&rw)) return 0U;
	StorageFlash_ConfigFromLegacy(config, afe, &rw);
	return StorageFlash_SaveConfigData(config);
}

UINT8 StorageFlash_LoadAfeData(UINT16 *values, UINT16 word_count)
{
	STORAGE_FLASH_CONFIG_DATA config;
	if ((values == 0) || (word_count != FLASH_STORAGE_AFE_WORD_COUNT)) return 0U;
	if (StorageFlash_LoadConfigData(&config))
	{
		memcpy(values, config.afe, sizeof(config.afe));
		return 1U;
	}
	if (!StorageFlash_LoadLegacyAfe(values)) return 0U;
	(void)StorageFlash_TryMigrateLegacyConfig(&config);
	return 1U;
}

UINT8 StorageFlash_SaveAfeData(const UINT16 *values, UINT16 word_count)
{
	STORAGE_FLASH_CONFIG_DATA config;
	STORAGE_FLASH_RW_PARAM_DATA rw;
	UINT8 result;
	if ((values == 0) || (word_count != FLASH_STORAGE_AFE_WORD_COUNT)) return 0U;
	if (StorageFlash_LoadConfigData(&config))
	{
		memcpy(config.afe, values, sizeof(config.afe));
		return StorageFlash_SaveConfigData(&config);
	}
	if (StorageFlash_LoadLegacyRw(&rw))
	{
		StorageFlash_ConfigFromLegacy(&config, values, &rw);
		return StorageFlash_SaveConfigData(&config);
	}
	StorageFlash_BeginWrite();
	result = StorageFlash_SavePair(FLASH_ADDR_STORAGE_AFE_SLOT_A, FLASH_ADDR_STORAGE_AFE_SLOT_B,
								   FLASH_STORAGE_MAGIC_AFE, (const UINT8 *)values,
								   (UINT16)(word_count * sizeof(UINT16)));
	StorageFlash_EndWrite();
	return result;
}

UINT8 StorageFlash_LoadRwParamData(STORAGE_FLASH_RW_PARAM_DATA *data)
{
	STORAGE_FLASH_CONFIG_DATA config;
	if (data == 0) return 0U;
	if (StorageFlash_LoadConfigData(&config))
	{
		memcpy(data->protect, config.protect, sizeof(data->protect));
		memcpy(data->other, config.other, sizeof(data->other));
		memcpy(data->reserved, config.reserved, sizeof(data->reserved));
		return 1U;
	}
	if (!StorageFlash_LoadLegacyRw(data)) return 0U;
	(void)StorageFlash_TryMigrateLegacyConfig(&config);
	return 1U;
}

UINT8 StorageFlash_SaveRwParamData(const STORAGE_FLASH_RW_PARAM_DATA *data)
{
	STORAGE_FLASH_CONFIG_DATA config;
	UINT16 afe[FLASH_STORAGE_AFE_WORD_COUNT];
	UINT8 result;
	if (data == 0) return 0U;
	if (StorageFlash_LoadConfigData(&config))
	{
		memcpy(config.protect, data->protect, sizeof(config.protect));
		memcpy(config.other, data->other, sizeof(config.other));
		memcpy(config.reserved, data->reserved, sizeof(config.reserved));
		return StorageFlash_SaveConfigData(&config);
	}
	if (StorageFlash_LoadLegacyAfe(afe))
	{
		StorageFlash_ConfigFromLegacy(&config, afe, data);
		return StorageFlash_SaveConfigData(&config);
	}
	StorageFlash_BeginWrite();
	result = StorageFlash_SavePair(FLASH_ADDR_STORAGE_RW_PARAM_SLOT_A, FLASH_ADDR_STORAGE_RW_PARAM_SLOT_B,
								   FLASH_STORAGE_MAGIC_RW_PARAM, (const UINT8 *)data, (UINT16)sizeof(*data));
	StorageFlash_EndWrite();
	return result;
}

UINT16 StorageFlash_GetConfigPolicyVersion(void)
{
	STORAGE_FLASH_CONFIG_DATA config;
	if (StorageFlash_LoadConfigData(&config)) return config.u16AppliedPolicyVersion;
	return FlashReadOneHalfWord(FLASH_ADDR_UPGRADE_PARAM_FLAG);
}

UINT8 StorageFlash_SetConfigPolicyVersion(UINT16 version)
{
	STORAGE_FLASH_CONFIG_DATA config;
	if (!StorageFlash_LoadConfigData(&config))
	{
		if (!StorageFlash_TryMigrateLegacyConfig(&config)) return 0U;
		if (!StorageFlash_LoadConfigData(&config)) return 0U;
	}
	if (config.u16AppliedPolicyVersion == version) return 1U;
	config.u16AppliedPolicyVersion = version;
	return StorageFlash_SaveConfigData(&config);
}

UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data)
{
	STORAGE_FLASH_SOC_DATA_V1 legacy_data;
	if (data == 0) return 0U;
	if (StorageFlash_LoadJournalPair(FLASH_ADDR_STORAGE_SOC_SLOT_A, FLASH_ADDR_STORAGE_SOC_SLOT_B,
									 FLASH_STORAGE_MAGIC_SOC, (UINT16)sizeof(STORAGE_FLASH_SOC_DATA), (UINT8 *)data) &&
		(data->u16FormatVersion == FLASH_STORAGE_SOC_DATA_VERSION_V2)) return 1U;
	if (!StorageFlash_LoadJournalPair(FLASH_ADDR_STORAGE_SOC_SLOT_A, FLASH_ADDR_STORAGE_SOC_SLOT_B,
									  FLASH_STORAGE_MAGIC_SOC, (UINT16)sizeof(STORAGE_FLASH_SOC_DATA_V1), (UINT8 *)&legacy_data)) return 0U;
	memset(data, 0, sizeof(*data));
	data->u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
	data->u16SocNow = legacy_data.u16SocNow;
	data->u16DsgSocInt = legacy_data.u16DsgSocInt;
	data->u32CycleTimes = legacy_data.u32CycleTimes;
	data->u16MaxErrorPercent = 100U;
	return 1U;
}

UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data)
{
	STORAGE_FLASH_SOC_DATA save_data;
	UINT8 result;
	if (data == 0) return 0U;
	save_data = *data;
	save_data.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
	StorageFlash_BeginWrite();
	result = StorageFlash_SaveJournalPair(FLASH_ADDR_STORAGE_SOC_SLOT_A, FLASH_ADDR_STORAGE_SOC_SLOT_B,
										  FLASH_STORAGE_MAGIC_SOC, (const UINT8 *)&save_data, (UINT16)sizeof(save_data));
	StorageFlash_EndWrite();
	return result;
}

UINT8 StorageFlash_LoadLogData(UINT8 *point, UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2])
{
	STORAGE_FLASH_LOG_DATA data;
	if ((point == 0) || (records == 0)) return 0U;
	if (!StorageFlash_LoadJournalPair(FLASH_ADDR_STORAGE_LOG_SLOT_A, FLASH_ADDR_STORAGE_LOG_SLOT_B,
									  FLASH_STORAGE_MAGIC_LOG, (UINT16)sizeof(data), (UINT8 *)&data)) return 0U;
	*point = data.point;
	memcpy(records, data.records, sizeof(data.records));
	return 1U;
}

UINT8 StorageFlash_SaveLogData(UINT8 point, const UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2])
{
	STORAGE_FLASH_LOG_DATA data, current_data;
	UINT8 result;
	if (records == 0) return 0U;
	memset(&data, 0, sizeof(data));
	data.point = point;
	memcpy(data.records, records, sizeof(data.records));
	if (StorageFlash_LoadJournalPair(FLASH_ADDR_STORAGE_LOG_SLOT_A, FLASH_ADDR_STORAGE_LOG_SLOT_B,
									 FLASH_STORAGE_MAGIC_LOG, (UINT16)sizeof(current_data), (UINT8 *)&current_data) &&
		memcmp(&current_data, &data, sizeof(data)) == 0) return 1U;
	StorageFlash_BeginWrite();
	result = StorageFlash_SaveJournalPair(FLASH_ADDR_STORAGE_LOG_SLOT_A, FLASH_ADDR_STORAGE_LOG_SLOT_B,
										  FLASH_STORAGE_MAGIC_LOG, (const UINT8 *)&data, (UINT16)sizeof(data));
	StorageFlash_EndWrite();
	return result;
}

UINT8 StorageFlash_LoadFactoryAgingData(STORAGE_FLASH_FACTORY_AGING_DATA *data)
{
	if (data == 0) return 0U;
	if (StorageFlash_LoadJournalPair(FLASH_ADDR_STORAGE_AGING_SLOT_A,
									  FLASH_ADDR_STORAGE_AGING_SLOT_B,
									  FLASH_STORAGE_MAGIC_FACTORY_AGING,
									  (UINT16)sizeof(*data),
									  (UINT8 *)data)) return 1U;

	/* Compatibility with the old one-halfword DONE marker on slot A. */
	if (FlashReadOneHalfWord(FLASH_ADDR_STORAGE_AGING_SLOT_A) == FLASH_FACTORY_AGING_DONE_VALUE)
	{
		memset(data, 0, sizeof(*data));
		data->u16State = FLASH_FACTORY_AGING_STATE_DONE;
		return 1U;
	}
	return 0U;
}

UINT8 StorageFlash_SaveFactoryAgingData(const STORAGE_FLASH_FACTORY_AGING_DATA *data)
{
	UINT8 result;
	if (data == 0) return 0U;
	StorageFlash_BeginWrite();
	result = StorageFlash_SaveJournalPair(FLASH_ADDR_STORAGE_AGING_SLOT_A,
										  FLASH_ADDR_STORAGE_AGING_SLOT_B,
										  FLASH_STORAGE_MAGIC_FACTORY_AGING,
										  (const UINT8 *)data,
										  (UINT16)sizeof(*data));
	StorageFlash_EndWrite();
	return result;
}

void StorageFlash_PrintBootCheck(void)
{
	UINT16 flash_size_kb = *((volatile UINT16 *)FLASH_SIZE_REG_ADDR);
	STORAGE_FLASH_CONFIG_DATA config;
	STORAGE_FLASH_FACTORY_AGING_DATA aging;
	printf("\r\n[FLASH_BOOT] flash_size_reg=%uKB page=%lu align=%u\r\n",
		   flash_size_kb, (unsigned long)FLASH_STORAGE_PAGE_SIZE, FLASH_STORAGE_RECORD_ALIGNMENT);
	if (flash_size_kb < 128U)
	{
		printf("[FLASH_BOOT] rear64 unavailable: skip 0x08010000+ storage check\r\n");
		return;
	}
	printf("[FLASH_BOOT] config=%u policy=0x%04X iap_mailbox=%u aging=%u\r\n",
		   StorageFlash_LoadConfigData(&config),
		   StorageFlash_GetConfigPolicyVersion(),
		   AppUpgrade_IsIapRequested(),
		   StorageFlash_LoadFactoryAgingData(&aging));
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
	if (AppUpgrade_RequestIap() != 0U) { __disable_fault_irq(); MCU_RESET(); }
}

void InitAreaSelect(void)
{
	if (AppUpgrade_IsIapRequested() != 0U) { __disable_fault_irq(); MCU_RESET(); }
}
