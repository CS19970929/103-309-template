#include "stm32f10x.h"

uint32_t SystemCoreClock = HSI_VALUE;
__I uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};

void SystemInit(void)
{
    RCC->CR |= (uint32_t)0x00000001U;

#ifndef STM32F10X_CL
    RCC->CFGR &= (uint32_t)0xF8FF0000U;
#else
    RCC->CFGR &= (uint32_t)0xF0FF0000U;
#endif

    RCC->CR &= (uint32_t)0xFEF6FFFFU;
    RCC->CR &= (uint32_t)0xFFFBFFFFU;
    RCC->CFGR &= (uint32_t)0xFF80FFFFU;

#ifdef STM32F10X_CL
    RCC->CR &= (uint32_t)0xEBFFFFFFU;
    RCC->CIR = 0x00FF0000U;
    RCC->CFGR2 = 0x00000000U;
#elif defined(STM32F10X_LD_VL) || defined(STM32F10X_MD_VL) || defined(STM32F10X_HD_VL)
    RCC->CIR = 0x009F0000U;
    RCC->CFGR2 = 0x00000000U;
#else
    RCC->CIR = 0x009F0000U;
#endif

    SCB->VTOR = FLASH_BASE;
    SystemCoreClock = HSI_VALUE;
}

void SystemCoreClockUpdate(void)
{
    SystemCoreClock = HSI_VALUE;
}
