#ifndef IAP_FLASH_H
#define IAP_FLASH_H

#include <stdint.h>

uint16_t IapFlash_ReadHalfWord(uint32_t address);
uint8_t IapFlash_IsImageSizeValid(uint32_t image_size);
uint8_t IapFlash_IsAppVectorValid(void);
uint8_t IapFlash_BeginUpgrade(uint32_t image_size);
uint8_t IapFlash_WriteBytes(uint32_t address, const uint8_t *data, uint32_t length);
void IapFlash_EndUpgrade(void);
uint8_t IapFlash_ClearUpdateFlagPreservePage(void);
void IapFlash_JumpToApp(void);

#endif
