#include "main.h"

#ifdef _IAP

// 个数为48，看.s文件相关vector个数，这个我没看过，后面学习一下怎么执行的
#if (defined(__CC_ARM))
__IO uint32_t VectorTable[48] __attribute__((at(0x20000000)));
#elif (defined(__ICCARM__))
#pragma location = 0x20000000
__no_init __IO uint32_t VectorTable[48];
#elif defined(__GNUC__)
__IO uint32_t VectorTable[48] __attribute__((section(".RAMVectorTable")));
#elif defined(__TASKING__)
__IO uint32_t VectorTable[48] __at(0x20000000);
#endif

#endif

void Init_IAPAPP(void)
{
#ifdef _IAP
	UINT8 i;
	// 就是这句话，导致无法debug，不按reset无法烧代码，具体原因后面再想，错误教程，函数都用错了
	// RCC_APB2PeriphResetCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE); // 中断向量表重映射使能

	for (i = 0; i < 48; i++)
	{
		VectorTable[i] = *(__IO uint32_t *)(APPLICATION_ADDRESS + (i << 2));
	}
	SYSCFG_MemoryRemapConfig(SYSCFG_MemoryRemap_SRAM); // Remap SRAM at 0x00000000
#endif
}

void App_FlashUpdateDet(void)
{
	if (1 == u8FlashUpdateFlag)
	{
		__delay_ms(10);
		u8FlashUpdateFlag = 0;
		MCU_RESET();
	}
}

UINT16 FlashReadOneHalfWord(UINT32 faddr)
{
	return *(vu16 *)faddr;
}

FLASH_Status FlashWriteOneHalfWord(uint32_t StartAddr, uint16_t Buffer)
{
	static uint16_t s_u16PageBuffer[PROJECT_CFG_FLASH_PAGE_SIZE / 2U];
	FLASH_Status result = FLASH_COMPLETE;
	uint32_t page_start;
	uint32_t index;
	uint32_t i;

	if ((StartAddr & 0x1U) != 0U)
	{
		return FLASH_ERROR_PROGRAM;
	}

	page_start = StartAddr & ~(PROJECT_CFG_FLASH_PAGE_SIZE - 1U);
	index = (StartAddr - page_start) >> 1;
	if (index >= (PROJECT_CFG_FLASH_PAGE_SIZE / 2U))
	{
		return FLASH_ERROR_PROGRAM;
	}

	if (FlashReadOneHalfWord(StartAddr) == Buffer)
	{
		return FLASH_COMPLETE;
	}

	for (i = 0; i < (PROJECT_CFG_FLASH_PAGE_SIZE / 2U); ++i)
	{
		s_u16PageBuffer[i] = FlashReadOneHalfWord(page_start + (i << 1));
	}
	s_u16PageBuffer[index] = Buffer;

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
	result = FLASH_ErasePage(page_start);
	if (result == FLASH_COMPLETE)
	{
		for (i = 0; i < (PROJECT_CFG_FLASH_PAGE_SIZE / 2U); ++i)
		{
			if (s_u16PageBuffer[i] != 0xFFFFU)
			{
				result = FLASH_ProgramHalfWord(page_start + (i << 1), s_u16PageBuffer[i]);
				if (result != FLASH_COMPLETE)
				{
					break;
				}
			}
		}
	}
	FLASH_Lock();
	return result;
}
