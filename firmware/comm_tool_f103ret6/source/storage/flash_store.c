#include "flash_store.h"
#include "comm_tool_config.h"
#include "crc16.h"
#include "stm32f10x_flash.h"
#include <string.h>

#define FLASH_STORE_MAGIC 0x43544657u
#define FLASH_STORE_VALID 0xA55A5AA5u

typedef struct {
    uint32_t magic;
    uint32_t valid;
    uint32_t image_size;
    uint16_t image_crc16;
    uint16_t calc_crc16;
    uint32_t generation;
} FlashStoreHeader;

static FlashStoreHeader s_header;
static uint16_t s_running_crc;
static uint32_t s_expected_size;
static uint32_t s_written_size;

static uint8_t FlashStore_IsRangeInCache(uint32_t address, uint32_t length)
{
    uint32_t end = address + length;
    uint32_t cache_end = COMM_TOOL_BMS_FW_CACHE_BASE + COMM_TOOL_BMS_FW_CACHE_SIZE;

    if (end < address) {
        return 0u;
    }
    if (address < COMM_TOOL_BMS_FW_CACHE_BASE) {
        return 0u;
    }
    if (end > cache_end) {
        return 0u;
    }
    return 1u;
}

static uint8_t FlashStore_EraseRange(uint32_t address, uint32_t length)
{
    uint32_t page;
    uint32_t end = address + length;

    FLASH_Unlock();
    for (page = address; page < end; page += COMM_TOOL_FLASH_PAGE_SIZE) {
        if (FLASH_ErasePage(page) != FLASH_COMPLETE) {
            FLASH_Lock();
            return 0u;
        }
    }
    FLASH_Lock();
    return 1u;
}

static uint8_t FlashStore_ProgramBytes(uint32_t address, const uint8_t *data, uint32_t length)
{
    uint32_t i;
    uint16_t halfword;

    FLASH_Unlock();
    for (i = 0u; i < length; i += 2u) {
        halfword = data[i];
        if ((i + 1u) < length) {
            halfword |= (uint16_t)data[i + 1u] << 8u;
        } else {
            halfword |= 0xFF00u;
        }
        if (FLASH_ProgramHalfWord(address + i, halfword) != FLASH_COMPLETE) {
            FLASH_Lock();
            return 0u;
        }
    }
    FLASH_Lock();
    return 1u;
}

static uint8_t FlashStore_WriteHeader(const FlashStoreHeader *header)
{
    if (!FlashStore_EraseRange(COMM_TOOL_UPGRADE_INDEX_BASE, COMM_TOOL_FLASH_PAGE_SIZE)) {
        return 0u;
    }

    return FlashStore_ProgramBytes(COMM_TOOL_UPGRADE_INDEX_BASE,
                                   (const uint8_t *)header,
                                   (uint32_t)sizeof(*header));
}

void FlashStore_Init(void)
{
    const FlashStoreHeader *flash_header = (const FlashStoreHeader *)COMM_TOOL_UPGRADE_INDEX_BASE;

    memset(&s_header, 0, sizeof(s_header));
    if ((flash_header->magic == FLASH_STORE_MAGIC) &&
        (flash_header->valid == FLASH_STORE_VALID)) {
        s_header = *flash_header;
    }
}

uint8_t FlashStore_Begin(uint32_t image_size, uint16_t image_crc16)
{
    FlashStoreHeader invalid_header;

    if ((image_size == 0u) || (image_size > COMM_TOOL_BMS_FW_CACHE_SIZE)) {
        return 0u;
    }

    memset(&invalid_header, 0, sizeof(invalid_header));
    invalid_header.magic = FLASH_STORE_MAGIC;
    invalid_header.valid = 0u;
    invalid_header.image_size = image_size;
    invalid_header.image_crc16 = image_crc16;

    if (!FlashStore_WriteHeader(&invalid_header)) {
        return 0u;
    }
    if (!FlashStore_EraseRange(COMM_TOOL_BMS_FW_CACHE_BASE, image_size)) {
        return 0u;
    }

    s_header = invalid_header;
    s_running_crc = 0xFFFFu;
    s_expected_size = image_size;
    s_written_size = 0u;
    return 1u;
}

uint8_t FlashStore_Write(uint32_t offset, const uint8_t *data, uint16_t length)
{
    uint32_t address = COMM_TOOL_BMS_FW_CACHE_BASE + offset;

    if ((data == 0) || (length == 0u)) {
        return 0u;
    }
    if ((offset & 0x1u) != 0u) {
        return 0u;
    }
    if (((length & 0x1u) != 0u) && ((offset + length) != s_expected_size)) {
        return 0u;
    }
    if ((offset + length) > s_expected_size) {
        return 0u;
    }
    if (offset != s_written_size) {
        return 0u;
    }
    if (!FlashStore_IsRangeInCache(address, length)) {
        return 0u;
    }
    if (!FlashStore_ProgramBytes(address, data, length)) {
        return 0u;
    }

    s_running_crc = Crc16_Update(s_running_crc, data, length);
    s_written_size += length;
    return 1u;
}

uint8_t FlashStore_Finalize(uint32_t image_size, uint16_t image_crc16)
{
    if ((image_size != s_expected_size) ||
        (image_crc16 != s_header.image_crc16) ||
        (s_written_size != image_size) ||
        (s_running_crc != image_crc16)) {
        return 0u;
    }

    s_header.magic = FLASH_STORE_MAGIC;
    s_header.valid = FLASH_STORE_VALID;
    s_header.image_size = image_size;
    s_header.image_crc16 = image_crc16;
    s_header.calc_crc16 = s_running_crc;
    s_header.generation++;

    return FlashStore_WriteHeader(&s_header);
}

void FlashStore_GetInfo(FlashStoreInfo *info)
{
    if (info == 0) {
        return;
    }
    info->valid = FlashStore_IsValid();
    info->image_size = s_header.image_size;
    info->image_crc16 = s_header.image_crc16;
    info->calc_crc16 = s_header.calc_crc16;
}

uint8_t FlashStore_IsValid(void)
{
    return ((s_header.magic == FLASH_STORE_MAGIC) &&
            (s_header.valid == FLASH_STORE_VALID) &&
            (s_header.image_size > 0u) &&
            (s_header.image_size <= COMM_TOOL_BMS_FW_CACHE_SIZE) &&
            (s_header.image_crc16 == s_header.calc_crc16)) ? 1u : 0u;
}
