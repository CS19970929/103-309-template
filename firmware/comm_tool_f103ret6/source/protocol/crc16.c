#include "crc16.h"

uint16_t Crc16_Update(uint16_t crc, const uint8_t *data, uint16_t length)
{
    uint16_t i;
    uint8_t bit;

    for (i = 0u; i < length; i++) {
        crc ^= data[i];
        for (bit = 0u; bit < 8u; bit++) {
            if ((crc & 0x0001u) != 0u) {
                crc = (uint16_t)((crc >> 1u) ^ 0xA001u);
            } else {
                crc >>= 1u;
            }
        }
    }

    return crc;
}

uint16_t Crc16_Modbus(const uint8_t *data, uint16_t length)
{
    return Crc16_Update(0xFFFFu, data, length);
}
