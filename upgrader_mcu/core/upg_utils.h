#ifndef UPG_UTILS_H
#define UPG_UTILS_H

#include <stdint.h>

uint16_t UpgReadBe16(const uint8_t *data);
uint32_t UpgReadBe32(const uint8_t *data);
int32_t UpgReadBeS32(const uint8_t *data);
void UpgWriteBe16(uint8_t *data, uint16_t value);
void UpgWriteBe32(uint8_t *data, uint32_t value);
void UpgWriteBeS32(uint8_t *data, int32_t value);

#endif
