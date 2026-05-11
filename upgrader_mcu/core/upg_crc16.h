#ifndef UPG_CRC16_H
#define UPG_CRC16_H

#include <stdint.h>

uint16_t UpgCrc16_Update(uint16_t crc, const uint8_t *data, uint32_t len);
uint16_t UpgCrc16_Calc(const uint8_t *data, uint32_t len);

#endif
