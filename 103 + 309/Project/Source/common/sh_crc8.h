#pragma once
#include <stdint.h>
#include <stddef.h>

/*
 * SH3673520 SPI CRC8
 * Poly: x^8 + x^2 + x + 1 => 0x07
 * Init: 0x00
 */
static inline uint8_t sh_crc8_update(uint8_t crc, uint8_t data)
{
    crc ^= data;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x80) {
            crc = (uint8_t)((crc << 1) ^ 0x07);
        } else {
            crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static inline uint8_t sh_crc8_calc(const uint8_t *buf, size_t len)
{
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc = sh_crc8_update(crc, buf[i]);
    }
    return crc;
}
