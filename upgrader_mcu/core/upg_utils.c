#include "upg_utils.h"

uint16_t UpgReadBe16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) | (uint16_t)data[1]);
}

uint32_t UpgReadBe32(const uint8_t *data)
{
    return (((uint32_t)data[0] << 24U) |
            ((uint32_t)data[1] << 16U) |
            ((uint32_t)data[2] << 8U) |
            (uint32_t)data[3]);
}

int32_t UpgReadBeS32(const uint8_t *data)
{
    return (int32_t)UpgReadBe32(data);
}

void UpgWriteBe16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)((value >> 8U) & 0xFFU);
    data[1] = (uint8_t)(value & 0xFFU);
}

void UpgWriteBe32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)((value >> 24U) & 0xFFU);
    data[1] = (uint8_t)((value >> 16U) & 0xFFU);
    data[2] = (uint8_t)((value >> 8U) & 0xFFU);
    data[3] = (uint8_t)(value & 0xFFU);
}

void UpgWriteBeS32(uint8_t *data, int32_t value)
{
    UpgWriteBe32(data, (uint32_t)value);
}
