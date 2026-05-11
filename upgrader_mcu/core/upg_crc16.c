#include "upg_crc16.h"

uint16_t UpgCrc16_Update(uint16_t crc, const uint8_t *data, uint32_t len)
{
    uint16_t value_crc = crc;
    uint32_t index;
    uint8_t bit;

    for (index = 0U; index < len; index++)
    {
        value_crc ^= data[index];
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((value_crc & 0x0001U) != 0U)
            {
                value_crc = (uint16_t)((value_crc >> 1U) ^ 0xA001U);
            }
            else
            {
                value_crc = (uint16_t)(value_crc >> 1U);
            }
        }
    }

    return value_crc;
}

uint16_t UpgCrc16_Calc(const uint8_t *data, uint32_t len)
{
    return UpgCrc16_Update(0xFFFFU, data, len);
}
