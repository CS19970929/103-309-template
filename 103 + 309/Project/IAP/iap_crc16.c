#include "iap_crc16.h"

uint16_t IapCrc16_Update(uint16_t crc, const uint8_t *data, uint32_t length)
{
    uint32_t count;
    uint8_t bit;
    uint16_t value_crc;

    value_crc = crc;
    for (count = 0U; count < length; count++)
    {
        value_crc ^= data[count];
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

uint16_t IapCrc16_Calc(const uint8_t *data, uint32_t length)
{
    return IapCrc16_Update(0xFFFFU, data, length);
}
