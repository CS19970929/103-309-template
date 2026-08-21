#include "main.h"
#include "DebugWatch.h"

#define FLASH_STORAGE_MAGIC_SOC             ((uint32_t)0x534F4331U)
#define FLASH_STORAGE_MAGIC_AFE             ((uint32_t)0x41464531U)
#define FLASH_STORAGE_MAGIC_RW_PARAM        ((uint32_t)0x52575031U)
#define FLASH_STORAGE_MAGIC_LOG             ((uint32_t)0x4C4F4731U)
#define FLASH_STORAGE_MAGIC_FACTORY_DATA    ((uint32_t)0x46414331U) /* FAC1 */
#define FLASH_STORAGE_MAGIC_FACTORY_AGING   ((uint32_t)0x41474531U)
#define FLASH_STORAGE_RECORD_VERSION        ((uint16_t)0x0001U)
#define FLASH_SIZE_REG_ADDR                 ((uint32_t)0x1FFFF7E0U)
#define FLASH_ERASE_RETRY_MAX               ((uint8_t)3U)
#define FLASH_STORAGE_BUFFER_SIZE           ((uint16_t)320U)

#define APP_UPGRADE_MAILBOX_ADDR            ((uint32_t)0x20004FE0U)
#define APP_UPGRADE_MAILBOX_MAGIC           ((uint32_t)0x49415031U)
#define APP_UPGRADE_MAILBOX_REQUEST         ((uint32_t)0x5AA55AA5U)

typedef struct
{
    uint16_t u16SocNow;
    uint16_t u16DsgSocInt;
    uint32_t u32CycleTimes;
} STORAGE_FLASH_SOC_DATA_V1;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t sequence;
    uint16_t crc;
    uint16_t reserved;
} STORAGE_FLASH_HEADER;

typedef struct
{
    uint8_t point;
    uint8_t reserved;
    uint8_t records[FLASH_STORAGE_LOG_RECORD_COUNT][2];
} STORAGE_FLASH_LOG_DATA;

typedef struct
{
    uint16_t u16FormatVersion;
    uint16_t u16ValidFlags;
    STORAGE_CALIB_DATA calibration;
    STORAGE_PRODUCT_ID_DATA product_id;
} STORAGE_FLASH_FACTORY_DATA;

typedef struct
{
    uint32_t magic;
    uint32_t magic_inv;
    uint32_t request;
    uint32_t request_inv;
    uint32_t crc;
} APP_UPGRADE_MAILBOX;

typedef enum
{
    STORAGE_FLASH_MODE_PAIR = 0,
    STORAGE_FLASH_MODE_JOURNAL_PAIR,
    STORAGE_FLASH_MODE_JOURNAL_SINGLE
} STORAGE_FLASH_MODE;

typedef struct
{
    uint32_t magic;
    uint32_t slot_a;
    uint32_t slot_b;
    uint16_t length;
    uint8_t mode;
} STORAGE_FLASH_OBJECT_DEF;

typedef struct FLASH_RUNTIME_TAG
{
    volatile uint8_t busy;
    uint8_t initialized;
    STORAGE_STATE state;
    uint16_t flash_size_kb;
    uint32_t erase_count;
    uint32_t write_count;
    uint32_t error_count;
} FLASH_RUNTIME;

static FLASH_RUNTIME s_flash;

static const STORAGE_FLASH_OBJECT_DEF s_obj_afe = {
    FLASH_STORAGE_MAGIC_AFE,
    FLASH_ADDR_STORAGE_AFE_SLOT_A,
    FLASH_ADDR_STORAGE_AFE_SLOT_B,
    (uint16_t)(FLASH_STORAGE_AFE_WORD_COUNT * sizeof(uint16_t)),
    STORAGE_FLASH_MODE_PAIR};

static const STORAGE_FLASH_OBJECT_DEF s_obj_rw_param = {
    FLASH_STORAGE_MAGIC_RW_PARAM,
    FLASH_ADDR_STORAGE_RW_PARAM_SLOT_A,
    FLASH_ADDR_STORAGE_RW_PARAM_SLOT_B,
    (uint16_t)sizeof(STORAGE_FLASH_RW_PARAM_DATA),
    STORAGE_FLASH_MODE_PAIR};

static const STORAGE_FLASH_OBJECT_DEF s_obj_soc = {
    FLASH_STORAGE_MAGIC_SOC,
    FLASH_ADDR_STORAGE_SOC_SLOT_A,
    FLASH_ADDR_STORAGE_SOC_SLOT_B,
    (uint16_t)sizeof(STORAGE_FLASH_SOC_DATA),
    STORAGE_FLASH_MODE_JOURNAL_PAIR};

static const STORAGE_FLASH_OBJECT_DEF s_obj_factory_data = {
    FLASH_STORAGE_MAGIC_FACTORY_DATA,
    FLASH_ADDR_STORAGE_FACTORY_SLOT_A,
    FLASH_ADDR_STORAGE_FACTORY_SLOT_B,
    (uint16_t)sizeof(STORAGE_FLASH_FACTORY_DATA),
    STORAGE_FLASH_MODE_PAIR};

static const STORAGE_FLASH_OBJECT_DEF s_obj_log = {
    FLASH_STORAGE_MAGIC_LOG,
    FLASH_ADDR_STORAGE_LOG_SLOT_A,
    FLASH_ADDR_STORAGE_LOG_SLOT_B,
    (uint16_t)sizeof(STORAGE_FLASH_LOG_DATA),
    STORAGE_FLASH_MODE_JOURNAL_PAIR};

static const STORAGE_FLASH_OBJECT_DEF s_obj_factory_aging = {
    FLASH_STORAGE_MAGIC_FACTORY_AGING,
    FLASH_ADDR_FACTORY_AGING_FLAG,
    0U,
    (uint16_t)sizeof(STORAGE_FLASH_FACTORY_AGING_DATA),
    STORAGE_FLASH_MODE_JOURNAL_SINGLE};

#if DEBUG_WATCH_ENABLED
void Flash_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
    watch->runtime.flash = &s_flash;
}
#endif

static void StorageFlash_EnsureInit(void)
{
    if (s_flash.initialized == 0U)
    {
        Storage_Init();
    }
}

void Storage_Init(void)
{
    uint16_t size_kb;

    size_kb = *((volatile uint16_t *)FLASH_SIZE_REG_ADDR);
    s_flash.flash_size_kb = size_kb;
    s_flash.initialized = 1U;

    if ((size_kb >= FLASH_STORAGE_REQUIRED_KB) && (size_kb <= 1024U))
    {
        s_flash.state = STORAGE_STATE_READY;
    }
#if FLASH_STORAGE_ALLOW_UNVERIFIED_REAR64
    else if ((size_kb >= 64U) && (size_kb < FLASH_STORAGE_REQUIRED_KB))
    {
        /* Legacy F103C8 compatibility: rear 64 KB is not guaranteed by ST. */
        s_flash.state = STORAGE_STATE_READY_UNVERIFIED_CAPACITY;
    }
#endif
    else
    {
        s_flash.state = STORAGE_STATE_UNSUPPORTED_FLASH;
    }
}

uint8_t Storage_IsReady(void)
{
    StorageFlash_EnsureInit();
    return (uint8_t)((s_flash.state == STORAGE_STATE_READY) ||
                     (s_flash.state == STORAGE_STATE_READY_UNVERIFIED_CAPACITY));
}

uint8_t Storage_IsVerifiedCapacity(void)
{
    StorageFlash_EnsureInit();
    return (uint8_t)(s_flash.state == STORAGE_STATE_READY);
}

uint16_t Storage_GetPhysicalFlashKb(void)
{
    StorageFlash_EnsureInit();
    return s_flash.flash_size_kb;
}

STORAGE_STATE Storage_GetState(void)
{
    StorageFlash_EnsureInit();
    return s_flash.state;
}

static uint32_t StorageFlash_AccessEnd(void)
{
    StorageFlash_EnsureInit();

    if (s_flash.state == STORAGE_STATE_READY)
    {
        return FLASH_STORAGE_FLASH_BASE + ((uint32_t)s_flash.flash_size_kb * 1024U);
    }
    if (s_flash.state == STORAGE_STATE_READY_UNVERIFIED_CAPACITY)
    {
        return FLASH_STORAGE_REQUIRED_END;
    }
    return FLASH_STORAGE_FLASH_BASE;
}

static uint8_t StorageFlash_AddressValid(uint32_t addr, uint32_t length)
{
    uint32_t end;
    uint32_t access_end;

    if (Storage_IsReady() == 0U)
    {
        return 0U;
    }
    if ((addr < FLASH_STORAGE_FLASH_BASE) || (length == 0U))
    {
        return 0U;
    }

    end = addr + length;
    if (end < addr)
    {
        return 0U;
    }

    access_end = StorageFlash_AccessEnd();
    return (uint8_t)(end <= access_end);
}

static void StorageFlash_BeginWrite(void)
{
    s_flash.busy = 1U;
}

static void StorageFlash_EndWrite(void)
{
    s_flash.busy = 0U;
}

uint8_t StorageFlash_IsBusy(void)
{
    return s_flash.busy;
}

uint8_t Storage_IsBusy(void)
{
    return StorageFlash_IsBusy();
}

static void StorageFlash_ReportError(void)
{
    ++s_flash.error_count;
    System_ERROR_UserCallback(ERROR_EEPROM_STORE);
}

/* Same Modbus CRC16 polynomial as Sci_CRC16RTU, but with a 16-bit length. */
static uint16_t StorageFlash_CalcCrc(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    uint16_t index;

    if ((data == 0) || (length == 0U))
    {
        return 0xFFFFU;
    }

    for (index = 0U; index < length; ++index)
    {
        uint8_t bit;
        crc ^= data[index];
        for (bit = 0U; bit < 8U; ++bit)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (uint16_t)((crc >> 1) ^ 0xA001U);
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static FLASH_Status FlashErasePageVerified(uint32_t page_addr)
{
    uint32_t offset;
    FLASH_Status result;

    if ((page_addr % FLASH_STORAGE_PAGE_SIZE) != 0U ||
        StorageFlash_AddressValid(page_addr, FLASH_STORAGE_PAGE_SIZE) == 0U)
    {
        return FLASH_ERROR_PG;
    }

    result = FLASH_ErasePage(page_addr);
    if (result != FLASH_COMPLETE)
    {
        return result;
    }
    ++s_flash.erase_count;

    for (offset = 0U; offset < FLASH_STORAGE_PAGE_SIZE; offset += 2U)
    {
        if (*((volatile uint16_t *)(page_addr + offset)) != 0xFFFFU)
        {
            return FLASH_ERROR_PG;
        }
    }

    return FLASH_COMPLETE;
}

static FLASH_Status FlashProgramHalfWordVerified(uint32_t addr, uint16_t data)
{
    FLASH_Status result;

    if (((addr & 1U) != 0U) ||
        (StorageFlash_AddressValid(addr, 2U) == 0U))
    {
        return FLASH_ERROR_PG;
    }

    result = FLASH_ProgramHalfWord(addr, data);
    if (result != FLASH_COMPLETE)
    {
        return result;
    }
    ++s_flash.write_count;

    if (*((volatile uint16_t *)addr) != data)
    {
        return FLASH_ERROR_PG;
    }

    return FLASH_COMPLETE;
}

static FLASH_Status FlashProgramBytesVerified(uint32_t addr,
                                               const uint8_t *data,
                                               uint16_t length)
{
    uint16_t offset = 0U;
    uint16_t half_word;
    FLASH_Status result = FLASH_COMPLETE;

    if ((data == 0) ||
        (length == 0U) ||
        (StorageFlash_AddressValid(addr, length) == 0U))
    {
        return FLASH_ERROR_PG;
    }

    while (offset < length)
    {
        half_word = data[offset];
        if ((uint16_t)(offset + 1U) < length)
        {
            half_word |= (uint16_t)((uint16_t)data[offset + 1U] << 8);
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
        offset = (uint16_t)(offset + 2U);
    }

    return result;
}

static uint16_t StorageFlash_RecordSpan(uint16_t payload_length)
{
    uint16_t record_span;

    record_span = (uint16_t)(sizeof(STORAGE_FLASH_HEADER) + payload_length);
    if ((record_span & 1U) != 0U)
    {
        ++record_span;
    }
    return record_span;
}

static uint8_t StorageFlash_IsAreaBlank(uint32_t addr, uint16_t length)
{
    uint16_t offset;

    if (StorageFlash_AddressValid(addr, length) == 0U)
    {
        return 0U;
    }

    for (offset = 0U; offset < length; offset = (uint16_t)(offset + 2U))
    {
        if (*((volatile uint16_t *)(addr + offset)) != 0xFFFFU)
        {
            return 0U;
        }
    }
    return 1U;
}

static uint8_t StorageFlash_ReadSlot(uint32_t slot_addr,
                                     uint32_t expect_magic,
                                     uint16_t expect_length,
                                     uint8_t *payload,
                                     uint32_t *sequence)
{
    const STORAGE_FLASH_HEADER *header;
    const uint8_t *payload_addr;
    uint16_t crc;

    if (StorageFlash_AddressValid(slot_addr,
                                 (uint32_t)sizeof(STORAGE_FLASH_HEADER) + expect_length) == 0U)
    {
        return 0U;
    }

    header = (const STORAGE_FLASH_HEADER *)slot_addr;
    payload_addr = (const uint8_t *)(slot_addr + sizeof(STORAGE_FLASH_HEADER));

    if ((header->magic != expect_magic) ||
        (header->version != FLASH_STORAGE_RECORD_VERSION) ||
        (header->length != expect_length))
    {
        return 0U;
    }

    crc = StorageFlash_CalcCrc(payload_addr, expect_length);
    if (crc != header->crc)
    {
        return 0U;
    }

    if (payload != 0)
    {
        memcpy(payload, payload_addr, expect_length);
    }
    if (sequence != 0)
    {
        *sequence = header->sequence;
    }
    return 1U;
}

static FLASH_Status StorageFlash_ProgramRecord(uint32_t record_addr,
                                                uint32_t magic,
                                                const uint8_t *payload,
                                                uint16_t length,
                                                uint32_t sequence)
{
    STORAGE_FLASH_HEADER header;
    FLASH_Status result;

    header.magic = magic;
    header.version = FLASH_STORAGE_RECORD_VERSION;
    header.length = length;
    header.sequence = sequence;
    header.crc = StorageFlash_CalcCrc(payload, length);
    header.reserved = 0xFFFFU;

    result = FlashProgramBytesVerified(record_addr,
                                       (const uint8_t *)&header,
                                       (uint16_t)sizeof(header));
    if (result == FLASH_COMPLETE)
    {
        result = FlashProgramBytesVerified(record_addr + sizeof(header), payload, length);
    }
    return result;
}

static uint8_t StorageFlash_LoadPair(uint32_t slot_a,
                                     uint32_t slot_b,
                                     uint32_t expect_magic,
                                     uint16_t expect_length,
                                     uint8_t *payload)
{
    uint8_t buffer_a[FLASH_STORAGE_BUFFER_SIZE];
    uint8_t buffer_b[FLASH_STORAGE_BUFFER_SIZE];
    const uint8_t *chosen;
    uint32_t seq_a = 0U;
    uint32_t seq_b = 0U;
    uint8_t valid_a;
    uint8_t valid_b;

    if ((payload == 0) || (expect_length > FLASH_STORAGE_BUFFER_SIZE))
    {
        return 0U;
    }

    valid_a = StorageFlash_ReadSlot(slot_a, expect_magic, expect_length, buffer_a, &seq_a);
    valid_b = StorageFlash_ReadSlot(slot_b, expect_magic, expect_length, buffer_b, &seq_b);

    if ((valid_a == 0U) && (valid_b == 0U))
    {
        return 0U;
    }

    if ((valid_a != 0U) && (valid_b != 0U))
    {
        chosen = (seq_a >= seq_b) ? buffer_a : buffer_b;
    }
    else
    {
        chosen = (valid_a != 0U) ? buffer_a : buffer_b;
    }

    memcpy(payload, chosen, expect_length);
    return 1U;
}

static FLASH_Status StorageFlash_WriteSlot(uint32_t slot_addr,
                                            uint32_t magic,
                                            const uint8_t *payload,
                                            uint16_t length,
                                            uint32_t sequence)
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

static uint8_t StorageFlash_SavePair(uint32_t slot_a,
                                     uint32_t slot_b,
                                     uint32_t magic,
                                     const uint8_t *payload,
                                     uint16_t length)
{
    uint8_t verify_buffer[FLASH_STORAGE_BUFFER_SIZE];
    uint8_t valid_a;
    uint8_t valid_b;
    uint32_t seq_a = 0U;
    uint32_t seq_b = 0U;
    uint32_t verify_sequence = 0U;
    uint32_t next_sequence = 1U;
    uint32_t target_slot = slot_a;
    FLASH_Status result;

    if ((payload == 0) || (length > FLASH_STORAGE_BUFFER_SIZE))
    {
        StorageFlash_ReportError();
        return 0U;
    }

    valid_a = StorageFlash_ReadSlot(slot_a, magic, length, verify_buffer, &seq_a);
    valid_b = StorageFlash_ReadSlot(slot_b, magic, length, verify_buffer, &seq_b);

    if ((valid_a != 0U) && (valid_b != 0U))
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
    else if (valid_a != 0U)
    {
        next_sequence = seq_a + 1U;
        target_slot = slot_b;
    }
    else if (valid_b != 0U)
    {
        next_sequence = seq_b + 1U;
        target_slot = slot_a;
    }

    result = StorageFlash_WriteSlot(target_slot, magic, payload, length, next_sequence);
    if (result != FLASH_COMPLETE)
    {
        StorageFlash_ReportError();
        return 0U;
    }

    if ((StorageFlash_ReadSlot(target_slot, magic, length,
                               verify_buffer, &verify_sequence) == 0U) ||
        (verify_sequence != next_sequence) ||
        (memcmp(verify_buffer, payload, length) != 0))
    {
        StorageFlash_ReportError();
        return 0U;
    }
    return 1U;
}

static uint8_t StorageFlash_LoadJournalPage(uint32_t slot_addr,
                                            uint32_t expect_magic,
                                            uint16_t expect_length,
                                            uint8_t *payload,
                                            uint32_t *sequence,
                                            uint32_t *next_addr)
{
    uint8_t temp_payload[FLASH_STORAGE_BUFFER_SIZE];
    uint16_t record_span;
    uint32_t offset;
    uint32_t record_addr;
    uint32_t latest_sequence = 0U;
    uint32_t current_sequence = 0U;
    uint32_t blank_addr = slot_addr + FLASH_STORAGE_PAGE_SIZE;
    uint8_t found = 0U;

    if (expect_length > FLASH_STORAGE_BUFFER_SIZE)
    {
        return 0U;
    }

    record_span = StorageFlash_RecordSpan(expect_length);
    if (record_span > FLASH_STORAGE_PAGE_SIZE)
    {
        return 0U;
    }

    for (offset = 0U;
         (offset + record_span) <= FLASH_STORAGE_PAGE_SIZE;
         offset += record_span)
    {
        record_addr = slot_addr + offset;
        if (StorageFlash_IsAreaBlank(record_addr, record_span) != 0U)
        {
            blank_addr = record_addr;
            break;
        }

        if (StorageFlash_ReadSlot(record_addr,
                                  expect_magic,
                                  expect_length,
                                  (payload != 0) ? temp_payload : 0,
                                  &current_sequence) != 0U)
        {
            if ((found == 0U) || (current_sequence >= latest_sequence))
            {
                latest_sequence = current_sequence;
                found = 1U;
                if (payload != 0)
                {
                    memcpy(payload, temp_payload, expect_length);
                }
            }
        }
    }

    if (sequence != 0)
    {
        *sequence = (found != 0U) ? latest_sequence : 0U;
    }
    if (next_addr != 0)
    {
        *next_addr = blank_addr;
    }
    return found;
}

static uint8_t StorageFlash_LoadJournalPair(uint32_t slot_a,
                                            uint32_t slot_b,
                                            uint32_t expect_magic,
                                            uint16_t expect_length,
                                            uint8_t *payload)
{
    uint8_t buffer_a[FLASH_STORAGE_BUFFER_SIZE];
    uint8_t buffer_b[FLASH_STORAGE_BUFFER_SIZE];
    const uint8_t *chosen;
    uint32_t seq_a = 0U;
    uint32_t seq_b = 0U;
    uint8_t valid_a;
    uint8_t valid_b;

    if ((payload == 0) || (expect_length > FLASH_STORAGE_BUFFER_SIZE))
    {
        return 0U;
    }

    valid_a = StorageFlash_LoadJournalPage(slot_a, expect_magic,
                                           expect_length, buffer_a, &seq_a, 0);
    valid_b = StorageFlash_LoadJournalPage(slot_b, expect_magic,
                                           expect_length, buffer_b, &seq_b, 0);

    if ((valid_a == 0U) && (valid_b == 0U))
    {
        return 0U;
    }

    if ((valid_a != 0U) && (valid_b != 0U))
    {
        chosen = (seq_a >= seq_b) ? buffer_a : buffer_b;
    }
    else
    {
        chosen = (valid_a != 0U) ? buffer_a : buffer_b;
    }

    memcpy(payload, chosen, expect_length);
    return 1U;
}

static uint8_t StorageFlash_SaveJournalPair(uint32_t slot_a,
                                            uint32_t slot_b,
                                            uint32_t magic,
                                            const uint8_t *payload,
                                            uint16_t length)
{
    uint8_t verify_buffer[FLASH_STORAGE_BUFFER_SIZE];
    uint16_t record_span;
    uint8_t valid_a;
    uint8_t valid_b;
    uint32_t seq_a = 0U;
    uint32_t seq_b = 0U;
    uint32_t next_addr_a = slot_a;
    uint32_t next_addr_b = slot_b;
    uint32_t next_sequence = 1U;
    uint32_t target_page = slot_a;
    uint32_t target_addr = slot_a;
    uint32_t verify_sequence = 0U;
    FLASH_Status result;
    uint8_t erase_target_page = 0U;

    if ((payload == 0) || (length > FLASH_STORAGE_BUFFER_SIZE))
    {
        StorageFlash_ReportError();
        return 0U;
    }

    record_span = StorageFlash_RecordSpan(length);
    if (record_span > FLASH_STORAGE_PAGE_SIZE)
    {
        StorageFlash_ReportError();
        return 0U;
    }

    valid_a = StorageFlash_LoadJournalPage(slot_a, magic, length, 0,
                                           &seq_a, &next_addr_a);
    valid_b = StorageFlash_LoadJournalPage(slot_b, magic, length, 0,
                                           &seq_b, &next_addr_b);

    if ((valid_a != 0U) && (valid_b != 0U))
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
    else if (valid_a != 0U)
    {
        next_sequence = seq_a + 1U;
        target_page = slot_a;
        target_addr = next_addr_a;
    }
    else if (valid_b != 0U)
    {
        next_sequence = seq_b + 1U;
        target_page = slot_b;
        target_addr = next_addr_b;
    }
    else
    {
        erase_target_page = 1U;
    }

    if ((target_addr + record_span) > (target_page + FLASH_STORAGE_PAGE_SIZE))
    {
        target_page = (target_page == slot_a) ? slot_b : slot_a;
        target_addr = target_page;
        erase_target_page = 1U;
    }

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    if (erase_target_page != 0U)
    {
        result = FlashErasePageVerified(target_page);
        if (result != FLASH_COMPLETE)
        {
            FLASH_Lock();
            StorageFlash_ReportError();
            return 0U;
        }
    }

    result = StorageFlash_ProgramRecord(target_addr, magic, payload,
                                        length, next_sequence);
    FLASH_Lock();
    if (result != FLASH_COMPLETE)
    {
        StorageFlash_ReportError();
        return 0U;
    }

    if ((StorageFlash_ReadSlot(target_addr, magic, length,
                               verify_buffer, &verify_sequence) == 0U) ||
        (verify_sequence != next_sequence) ||
        (memcmp(verify_buffer, payload, length) != 0))
    {
        StorageFlash_ReportError();
        return 0U;
    }
    return 1U;
}

static uint8_t StorageFlash_SaveJournalPage(uint32_t slot_addr,
                                            uint32_t magic,
                                            const uint8_t *payload,
                                            uint16_t length)
{
    uint8_t verify_buffer[FLASH_STORAGE_BUFFER_SIZE];
    uint16_t record_span;
    uint8_t valid;
    uint32_t sequence = 0U;
    uint32_t next_addr = slot_addr;
    uint32_t next_sequence = 1U;
    FLASH_Status result;
    uint8_t erase_page = 0U;

    if ((payload == 0) || (length > FLASH_STORAGE_BUFFER_SIZE))
    {
        StorageFlash_ReportError();
        return 0U;
    }

    record_span = StorageFlash_RecordSpan(length);
    if (record_span > FLASH_STORAGE_PAGE_SIZE)
    {
        StorageFlash_ReportError();
        return 0U;
    }

    valid = StorageFlash_LoadJournalPage(slot_addr, magic, length,
                                         0, &sequence, &next_addr);
    if (valid != 0U)
    {
        next_sequence = sequence + 1U;
    }
    else
    {
        erase_page = (StorageFlash_IsAreaBlank(slot_addr,
                     (uint16_t)FLASH_STORAGE_PAGE_SIZE) != 0U) ? 0U : 1U;
        next_addr = slot_addr;
    }

    if ((next_addr + record_span) > (slot_addr + FLASH_STORAGE_PAGE_SIZE))
    {
        erase_page = 1U;
        next_addr = slot_addr;
    }

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    if (erase_page != 0U)
    {
        result = FlashErasePageVerified(slot_addr);
        if (result != FLASH_COMPLETE)
        {
            FLASH_Lock();
            StorageFlash_ReportError();
            return 0U;
        }
    }

    result = StorageFlash_ProgramRecord(next_addr, magic, payload,
                                        length, next_sequence);
    FLASH_Lock();
    if (result != FLASH_COMPLETE)
    {
        StorageFlash_ReportError();
        return 0U;
    }

    if ((StorageFlash_ReadSlot(next_addr, magic, length,
                               verify_buffer, &sequence) == 0U) ||
        (sequence != next_sequence) ||
        (memcmp(verify_buffer, payload, length) != 0))
    {
        StorageFlash_ReportError();
        return 0U;
    }
    return 1U;
}

static uint8_t StorageFlash_LoadObject(const STORAGE_FLASH_OBJECT_DEF *object,
                                       uint8_t *payload)
{
    if ((object == 0) || (payload == 0) || (Storage_IsReady() == 0U))
    {
        return 0U;
    }

    switch ((STORAGE_FLASH_MODE)object->mode)
    {
    case STORAGE_FLASH_MODE_PAIR:
        return StorageFlash_LoadPair(object->slot_a, object->slot_b,
                                     object->magic, object->length, payload);
    case STORAGE_FLASH_MODE_JOURNAL_PAIR:
        return StorageFlash_LoadJournalPair(object->slot_a, object->slot_b,
                                            object->magic, object->length, payload);
    case STORAGE_FLASH_MODE_JOURNAL_SINGLE:
        return StorageFlash_LoadJournalPage(object->slot_a, object->magic,
                                            object->length, payload, 0, 0);
    default:
        return 0U;
    }
}

static uint8_t StorageFlash_SaveObject(const STORAGE_FLASH_OBJECT_DEF *object,
                                       const uint8_t *payload)
{
    uint8_t result;

    if ((object == 0) || (payload == 0) || (Storage_IsReady() == 0U))
    {
        StorageFlash_ReportError();
        return 0U;
    }

    StorageFlash_BeginWrite();
    switch ((STORAGE_FLASH_MODE)object->mode)
    {
    case STORAGE_FLASH_MODE_PAIR:
        result = StorageFlash_SavePair(object->slot_a, object->slot_b,
                                       object->magic, payload, object->length);
        break;
    case STORAGE_FLASH_MODE_JOURNAL_PAIR:
        result = StorageFlash_SaveJournalPair(object->slot_a, object->slot_b,
                                              object->magic, payload, object->length);
        break;
    case STORAGE_FLASH_MODE_JOURNAL_SINGLE:
        result = StorageFlash_SaveJournalPage(object->slot_a,
                                              object->magic, payload, object->length);
        break;
    default:
        result = 0U;
        StorageFlash_ReportError();
        break;
    }
    StorageFlash_EndWrite();
    return result;
}

FLASH_Status FlashWriteOneHalfWord(uint32_t StartAddr, uint16_t Buffer)
{
    FLASH_Status result;
    uint8_t retry;

    if (StorageFlash_AddressValid(StartAddr, FLASH_STORAGE_PAGE_SIZE) == 0U)
    {
        StorageFlash_ReportError();
        return FLASH_ERROR_PG;
    }

    StorageFlash_BeginWrite();
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    result = FLASH_ERROR_PG;
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
    if (result != FLASH_COMPLETE)
    {
        StorageFlash_ReportError();
    }
    return result;
}

uint16_t FlashReadOneHalfWord(uint32_t faddr)
{
    if (StorageFlash_AddressValid(faddr, 2U) == 0U)
    {
        return 0xFFFFU;
    }
    return *((volatile uint16_t *)faddr);
}

static volatile APP_UPGRADE_MAILBOX *AppUpgrade_Mailbox(void)
{
    return (volatile APP_UPGRADE_MAILBOX *)APP_UPGRADE_MAILBOX_ADDR;
}

static uint32_t AppUpgrade_MailboxCrc(uint32_t magic, uint32_t request)
{
    return magic ^ request ^ 0xA5A55A5AU;
}

static uint8_t AppUpgrade_IsIapRequested(void)
{
    volatile APP_UPGRADE_MAILBOX *mailbox = AppUpgrade_Mailbox();

    if ((mailbox->magic != APP_UPGRADE_MAILBOX_MAGIC) ||
        (mailbox->magic_inv != (uint32_t)~APP_UPGRADE_MAILBOX_MAGIC) ||
        (mailbox->request != APP_UPGRADE_MAILBOX_REQUEST) ||
        (mailbox->request_inv != (uint32_t)~APP_UPGRADE_MAILBOX_REQUEST) ||
        (mailbox->crc != AppUpgrade_MailboxCrc(APP_UPGRADE_MAILBOX_MAGIC,
                                               APP_UPGRADE_MAILBOX_REQUEST)))
    {
        return 0U;
    }
    return 1U;
}

uint8_t AppUpgrade_RequestIap(void)
{
    volatile APP_UPGRADE_MAILBOX *mailbox = AppUpgrade_Mailbox();

    mailbox->magic = APP_UPGRADE_MAILBOX_MAGIC;
    mailbox->magic_inv = (uint32_t)~APP_UPGRADE_MAILBOX_MAGIC;
    mailbox->request = APP_UPGRADE_MAILBOX_REQUEST;
    mailbox->request_inv = (uint32_t)~APP_UPGRADE_MAILBOX_REQUEST;
    mailbox->crc = AppUpgrade_MailboxCrc(APP_UPGRADE_MAILBOX_MAGIC,
                                         APP_UPGRADE_MAILBOX_REQUEST);
    return AppUpgrade_IsIapRequested();
}

static uint8_t StorageFlash_LoadFactoryData(STORAGE_FLASH_FACTORY_DATA *data)
{
    if (data == 0)
    {
        return 0U;
    }
    if (StorageFlash_LoadObject(&s_obj_factory_data, (uint8_t *)data) == 0U)
    {
        return 0U;
    }
    return (uint8_t)(data->u16FormatVersion == STORAGE_FACTORY_DATA_VERSION);
}

static uint8_t StorageFlash_SaveFactoryData(const STORAGE_FLASH_FACTORY_DATA *data)
{
    STORAGE_FLASH_FACTORY_DATA save_data;

    if (data == 0)
    {
        StorageFlash_ReportError();
        return 0U;
    }

    save_data = *data;
    save_data.u16FormatVersion = STORAGE_FACTORY_DATA_VERSION;
    return StorageFlash_SaveObject(&s_obj_factory_data, (const uint8_t *)&save_data);
}

uint8_t StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data)
{
    STORAGE_FLASH_SOC_DATA_V1 legacy_data;

    if (data == 0)
    {
        return 0U;
    }

    if (StorageFlash_LoadObject(&s_obj_soc, (uint8_t *)data) != 0U)
    {
        if (data->u16FormatVersion == FLASH_STORAGE_SOC_DATA_VERSION_V2)
        {
            return 1U;
        }
    }

    if (StorageFlash_LoadJournalPair(FLASH_ADDR_STORAGE_SOC_SLOT_A,
                                     FLASH_ADDR_STORAGE_SOC_SLOT_B,
                                     FLASH_STORAGE_MAGIC_SOC,
                                     (uint16_t)sizeof(legacy_data),
                                     (uint8_t *)&legacy_data) == 0U)
    {
        return 0U;
    }

    memset(data, 0, sizeof(*data));
    data->u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
    data->u16SocNow = legacy_data.u16SocNow;
    data->u16DsgSocInt = legacy_data.u16DsgSocInt;
    data->u32CycleTimes = legacy_data.u32CycleTimes;
    data->u16MaxErrorPercent = 100U;
    return 1U;
}

uint8_t StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data)
{
    STORAGE_FLASH_SOC_DATA save_data;

    if (data == 0)
    {
        StorageFlash_ReportError();
        return 0U;
    }

    save_data = *data;
    save_data.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
    return StorageFlash_SaveObject(&s_obj_soc, (const uint8_t *)&save_data);
}

uint8_t StorageFlash_LoadAfeData(uint16_t *values, uint16_t word_count)
{
    if ((values == 0) || (word_count != FLASH_STORAGE_AFE_WORD_COUNT))
    {
        return 0U;
    }
    return StorageFlash_LoadObject(&s_obj_afe, (uint8_t *)values);
}

uint8_t StorageFlash_SaveAfeData(const uint16_t *values, uint16_t word_count)
{
    if ((values == 0) || (word_count != FLASH_STORAGE_AFE_WORD_COUNT))
    {
        StorageFlash_ReportError();
        return 0U;
    }
    return StorageFlash_SaveObject(&s_obj_afe, (const uint8_t *)values);
}

uint8_t StorageFlash_LoadRwParamData(STORAGE_FLASH_RW_PARAM_DATA *data)
{
    if (data == 0)
    {
        return 0U;
    }
    return StorageFlash_LoadObject(&s_obj_rw_param, (uint8_t *)data);
}

uint8_t StorageFlash_SaveRwParamData(const STORAGE_FLASH_RW_PARAM_DATA *data)
{
    if (data == 0)
    {
        StorageFlash_ReportError();
        return 0U;
    }
    return StorageFlash_SaveObject(&s_obj_rw_param, (const uint8_t *)data);
}

uint8_t StorageFlash_LoadLogData(uint8_t *point,
                                 uint8_t records[FLASH_STORAGE_LOG_RECORD_COUNT][2])
{
    STORAGE_FLASH_LOG_DATA data;

    if ((point == 0) || (records == 0))
    {
        return 0U;
    }
    if (StorageFlash_LoadObject(&s_obj_log, (uint8_t *)&data) == 0U)
    {
        return 0U;
    }

    *point = data.point;
    memcpy(records, data.records, sizeof(data.records));
    return 1U;
}

uint8_t StorageFlash_SaveLogData(uint8_t point,
                                 const uint8_t records[FLASH_STORAGE_LOG_RECORD_COUNT][2])
{
    STORAGE_FLASH_LOG_DATA data;
    STORAGE_FLASH_LOG_DATA current_data;

    if (records == 0)
    {
        StorageFlash_ReportError();
        return 0U;
    }

    memset(&data, 0, sizeof(data));
    data.point = point;
    memcpy(data.records, records, sizeof(data.records));

    if ((StorageFlash_LoadObject(&s_obj_log, (uint8_t *)&current_data) != 0U) &&
        (memcmp(&current_data, &data, sizeof(data)) == 0))
    {
        return 1U;
    }

    return StorageFlash_SaveObject(&s_obj_log, (const uint8_t *)&data);
}

uint8_t StorageFlash_LoadFactoryAgingData(STORAGE_FLASH_FACTORY_AGING_DATA *data)
{
    if (data == 0)
    {
        return 0U;
    }

    if (StorageFlash_LoadObject(&s_obj_factory_aging, (uint8_t *)data) != 0U)
    {
        return 1U;
    }

    if (FlashReadOneHalfWord(FLASH_ADDR_FACTORY_AGING_FLAG) ==
        FLASH_FACTORY_AGING_DONE_VALUE)
    {
        memset(data, 0, sizeof(*data));
        data->u16State = FLASH_FACTORY_AGING_STATE_DONE;
        return 1U;
    }
    return 0U;
}

uint8_t StorageFlash_SaveFactoryAgingData(const STORAGE_FLASH_FACTORY_AGING_DATA *data)
{
    if (data == 0)
    {
        StorageFlash_ReportError();
        return 0U;
    }
    return StorageFlash_SaveObject(&s_obj_factory_aging, (const uint8_t *)data);
}

/* Public storage facade. */
uint8_t Storage_LoadSocData(STORAGE_SOC_DATA *data)
{
    return StorageFlash_LoadSocData(data);
}

uint8_t Storage_SaveSocData(const STORAGE_SOC_DATA *data)
{
    return StorageFlash_SaveSocData(data);
}

uint8_t Storage_LoadAfeData(uint16_t *values, uint16_t word_count)
{
    return StorageFlash_LoadAfeData(values, word_count);
}

uint8_t Storage_SaveAfeData(const uint16_t *values, uint16_t word_count)
{
    return StorageFlash_SaveAfeData(values, word_count);
}

uint8_t Storage_LoadRwParamData(STORAGE_RW_PARAM_DATA *data)
{
    return StorageFlash_LoadRwParamData(data);
}

uint8_t Storage_SaveRwParamData(const STORAGE_RW_PARAM_DATA *data)
{
    return StorageFlash_SaveRwParamData(data);
}

uint8_t Storage_LoadCalibrationData(STORAGE_CALIB_DATA *data)
{
    STORAGE_FLASH_FACTORY_DATA factory_data;

    if (data == 0)
    {
        return 0U;
    }
    if ((StorageFlash_LoadFactoryData(&factory_data) == 0U) ||
        ((factory_data.u16ValidFlags & STORAGE_FACTORY_DATA_VALID_CALIB) == 0U))
    {
        return 0U;
    }

    *data = factory_data.calibration;
    return 1U;
}

uint8_t Storage_SaveCalibrationData(const STORAGE_CALIB_DATA *data)
{
    STORAGE_FLASH_FACTORY_DATA factory_data;

    if (data == 0)
    {
        StorageFlash_ReportError();
        return 0U;
    }

    if (StorageFlash_LoadFactoryData(&factory_data) == 0U)
    {
        memset(&factory_data, 0, sizeof(factory_data));
        factory_data.u16FormatVersion = STORAGE_FACTORY_DATA_VERSION;
    }
    factory_data.calibration = *data;
    factory_data.u16ValidFlags |= STORAGE_FACTORY_DATA_VALID_CALIB;
    return StorageFlash_SaveFactoryData(&factory_data);
}

uint8_t Storage_LoadProductIdData(STORAGE_PRODUCT_ID_DATA *data)
{
    STORAGE_FLASH_FACTORY_DATA factory_data;

    if (data == 0)
    {
        return 0U;
    }
    if ((StorageFlash_LoadFactoryData(&factory_data) == 0U) ||
        ((factory_data.u16ValidFlags & STORAGE_FACTORY_DATA_VALID_PRODUCT_ID) == 0U))
    {
        return 0U;
    }

    *data = factory_data.product_id;
    return 1U;
}

uint8_t Storage_SaveProductIdData(const STORAGE_PRODUCT_ID_DATA *data)
{
    STORAGE_FLASH_FACTORY_DATA factory_data;

    if (data == 0)
    {
        StorageFlash_ReportError();
        return 0U;
    }

    if (StorageFlash_LoadFactoryData(&factory_data) == 0U)
    {
        memset(&factory_data, 0, sizeof(factory_data));
        factory_data.u16FormatVersion = STORAGE_FACTORY_DATA_VERSION;
    }
    factory_data.product_id = *data;
    factory_data.u16ValidFlags |= STORAGE_FACTORY_DATA_VALID_PRODUCT_ID;
    return StorageFlash_SaveFactoryData(&factory_data);
}

uint8_t Storage_LoadLogData(uint8_t *point,
                            uint8_t records[STORAGE_LOG_RECORD_COUNT][2])
{
    return StorageFlash_LoadLogData(point, records);
}

uint8_t Storage_SaveLogData(uint8_t point,
                            const uint8_t records[STORAGE_LOG_RECORD_COUNT][2])
{
    return StorageFlash_SaveLogData(point, records);
}

uint8_t Storage_LoadFactoryAgingData(STORAGE_FACTORY_AGING_DATA *data)
{
    return StorageFlash_LoadFactoryAgingData(data);
}

uint8_t Storage_SaveFactoryAgingData(const STORAGE_FACTORY_AGING_DATA *data)
{
    return StorageFlash_SaveFactoryAgingData(data);
}

static char StorageFlash_SelectLabel(uint8_t valid_a, uint32_t seq_a,
                                     uint8_t valid_b, uint32_t seq_b)
{
    if ((valid_a != 0U) && (valid_b != 0U))
    {
        return (seq_a >= seq_b) ? 'A' : 'B';
    }
    if (valid_a != 0U)
    {
        return 'A';
    }
    if (valid_b != 0U)
    {
        return 'B';
    }
    return '-';
}

void StorageFlash_PrintBootCheck(void)
{
    uint8_t afe_valid_a;
    uint8_t afe_valid_b;
    uint8_t rw_valid_a;
    uint8_t rw_valid_b;
    uint8_t factory_valid_a;
    uint8_t factory_valid_b;
    uint8_t soc_valid_a;
    uint8_t soc_valid_b;
    uint32_t afe_seq_a = 0U;
    uint32_t afe_seq_b = 0U;
    uint32_t rw_seq_a = 0U;
    uint32_t rw_seq_b = 0U;
    uint32_t factory_seq_a = 0U;
    uint32_t factory_seq_b = 0U;
    uint32_t soc_seq_a = 0U;
    uint32_t soc_seq_b = 0U;
    uint32_t soc_next_a = FLASH_ADDR_STORAGE_SOC_SLOT_A;
    uint32_t soc_next_b = FLASH_ADDR_STORAGE_SOC_SLOT_B;
    uint16_t upgrade_flag;
    uint16_t aging_flag;
    STORAGE_FLASH_FACTORY_AGING_DATA aging_data;
    uint8_t aging_valid;
    uint8_t iap_request_pending;

    StorageFlash_EnsureInit();
    printf("\r\n[FLASH_BOOT] flash=%uKB state=%u verified=%u page=%lu\r\n",
           s_flash.flash_size_kb,
           (unsigned)s_flash.state,
           (unsigned)Storage_IsVerifiedCapacity(),
           (unsigned long)FLASH_STORAGE_PAGE_SIZE);

    if (Storage_IsReady() == 0U)
    {
        printf("[FLASH_BOOT] persistent storage unavailable\r\n");
        return;
    }

    afe_valid_a = StorageFlash_ReadSlot(s_obj_afe.slot_a, s_obj_afe.magic,
                                        s_obj_afe.length, 0, &afe_seq_a);
    afe_valid_b = StorageFlash_ReadSlot(s_obj_afe.slot_b, s_obj_afe.magic,
                                        s_obj_afe.length, 0, &afe_seq_b);
    rw_valid_a = StorageFlash_ReadSlot(s_obj_rw_param.slot_a, s_obj_rw_param.magic,
                                       s_obj_rw_param.length, 0, &rw_seq_a);
    rw_valid_b = StorageFlash_ReadSlot(s_obj_rw_param.slot_b, s_obj_rw_param.magic,
                                       s_obj_rw_param.length, 0, &rw_seq_b);
    factory_valid_a = StorageFlash_ReadSlot(s_obj_factory_data.slot_a, s_obj_factory_data.magic,
                                            s_obj_factory_data.length, 0, &factory_seq_a);
    factory_valid_b = StorageFlash_ReadSlot(s_obj_factory_data.slot_b, s_obj_factory_data.magic,
                                            s_obj_factory_data.length, 0, &factory_seq_b);

    soc_valid_a = StorageFlash_LoadJournalPage(s_obj_soc.slot_a, s_obj_soc.magic,
                                               s_obj_soc.length, 0,
                                               &soc_seq_a, &soc_next_a);
    soc_valid_b = StorageFlash_LoadJournalPage(s_obj_soc.slot_b, s_obj_soc.magic,
                                               s_obj_soc.length, 0,
                                               &soc_seq_b, &soc_next_b);
    if (soc_valid_a == 0U)
    {
        soc_valid_a = StorageFlash_LoadJournalPage(s_obj_soc.slot_a, s_obj_soc.magic,
                          (uint16_t)sizeof(STORAGE_FLASH_SOC_DATA_V1), 0,
                          &soc_seq_a, &soc_next_a);
    }
    if (soc_valid_b == 0U)
    {
        soc_valid_b = StorageFlash_LoadJournalPage(s_obj_soc.slot_b, s_obj_soc.magic,
                          (uint16_t)sizeof(STORAGE_FLASH_SOC_DATA_V1), 0,
                          &soc_seq_b, &soc_next_b);
    }

    printf("[FLASH_BOOT] AFE A=%u seq=%lu B=%u seq=%lu selected=%c\r\n",
           afe_valid_a, (unsigned long)afe_seq_a,
           afe_valid_b, (unsigned long)afe_seq_b,
           StorageFlash_SelectLabel(afe_valid_a, afe_seq_a, afe_valid_b, afe_seq_b));
    printf("[FLASH_BOOT] RW A=%u seq=%lu B=%u seq=%lu selected=%c\r\n",
           rw_valid_a, (unsigned long)rw_seq_a,
           rw_valid_b, (unsigned long)rw_seq_b,
           StorageFlash_SelectLabel(rw_valid_a, rw_seq_a, rw_valid_b, rw_seq_b));
    printf("[FLASH_BOOT] FACTORY A=%u seq=%lu B=%u seq=%lu selected=%c\r\n",
           factory_valid_a, (unsigned long)factory_seq_a,
           factory_valid_b, (unsigned long)factory_seq_b,
           StorageFlash_SelectLabel(factory_valid_a, factory_seq_a,
                                    factory_valid_b, factory_seq_b));
    printf("[FLASH_BOOT] SOC A=%u seq=%lu next=0x%04lX B=%u seq=%lu next=0x%04lX selected=%c\r\n",
           soc_valid_a, (unsigned long)soc_seq_a,
           (unsigned long)(soc_next_a - s_obj_soc.slot_a),
           soc_valid_b, (unsigned long)soc_seq_b,
           (unsigned long)(soc_next_b - s_obj_soc.slot_b),
           StorageFlash_SelectLabel(soc_valid_a, soc_seq_a, soc_valid_b, soc_seq_b));

    iap_request_pending = AppUpgrade_IsIapRequested();
    upgrade_flag = FlashReadOneHalfWord(FLASH_ADDR_UPGRADE_PARAM_FLAG);
    aging_flag = FlashReadOneHalfWord(FLASH_ADDR_FACTORY_AGING_FLAG);
    aging_valid = StorageFlash_LoadFactoryAgingData(&aging_data);
    printf("[FLASH_BOOT] iap=%u upgrade=0x%04X aging_raw=0x%04X erase=%lu write=%lu err=%lu\r\n",
           iap_request_pending, upgrade_flag, aging_flag,
           (unsigned long)s_flash.erase_count,
           (unsigned long)s_flash.write_count,
           (unsigned long)s_flash.error_count);
    printf("[FLASH_BOOT] aging valid=%u state=0x%04X elapsed10ms=%lu\r\n",
           aging_valid,
           aging_valid ? aging_data.u16State : 0xFFFFU,
           aging_valid ? (unsigned long)aging_data.u32Elapsed10ms : 0UL);
}

void App_FlashUpdate(void)
{
    if (u8FlashUpdateFlag == 1U)
    {
        SH367309_DriverMos_Ctrl(GPIO_CHG, 0U);
        SH367309_DriverMos_Ctrl(GPIO_DSG, 0U);
        __delay_ms(10);
        u8FlashUpdateFlag = 0U;
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
