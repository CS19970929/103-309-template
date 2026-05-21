#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>

uint16_t Crc16_Modbus(const uint8_t *data, uint16_t length);
uint16_t Crc16_Update(uint16_t crc, const uint8_t *data, uint16_t length);

#endif
