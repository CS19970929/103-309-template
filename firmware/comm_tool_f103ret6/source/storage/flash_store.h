#ifndef FLASH_STORE_H
#define FLASH_STORE_H

#include <stdint.h>

typedef struct {
    uint32_t valid;
    uint32_t image_size;
    uint16_t image_crc16;
    uint16_t calc_crc16;
} FlashStoreInfo;

void FlashStore_Init(void);
uint8_t FlashStore_Begin(uint32_t image_size, uint16_t image_crc16);
uint8_t FlashStore_Write(uint32_t offset, const uint8_t *data, uint16_t length);
uint8_t FlashStore_Finalize(uint32_t image_size, uint16_t image_crc16);
void FlashStore_GetInfo(FlashStoreInfo *info);
uint8_t FlashStore_IsValid(void);

#endif
