#ifndef IAP_CRC16_H
#define IAP_CRC16_H

#include <stdint.h>

uint16_t IapCrc16_Update(uint16_t crc, const uint8_t *data, uint32_t length);
uint16_t IapCrc16_Calc(const uint8_t *data, uint32_t length);

#endif
