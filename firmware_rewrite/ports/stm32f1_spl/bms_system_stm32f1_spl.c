#include "stm32f10x.h"

#define BMS_VECTOR_OFFSET 0x4800u

uint32_t SystemCoreClock = HSI_VALUE;
__I uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};

void SystemCoreClockUpdate(void)
{
    uint32_t prescaler;
    uint32_t clock_source = RCC->CFGR & RCC_CFGR_SWS;

    if (clock_source == RCC_CFGR_SWS_HSE) {
        SystemCoreClock = HSE_VALUE;
    } else if (clock_source == RCC_CFGR_SWS_PLL) {
        SystemCoreClock = HSE_VALUE;
    } else {
        SystemCoreClock = HSI_VALUE;
    }

    prescaler = AHBPrescTable[(RCC->CFGR & RCC_CFGR_HPRE) >> 4];
    SystemCoreClock >>= prescaler;
}

void SystemInit(void)
{
    RCC->CR |= RCC_CR_HSION;
    RCC->CFGR &= (uint32_t)0xF8FF0000u;
    RCC->CR &= (uint32_t)0xFEF6FFFFu;
    RCC->CR &= (uint32_t)0xFFFBFFFFu;
    RCC->CFGR &= (uint32_t)0xFF80FFFFu;
    RCC->CIR = 0x009F0000u;

    SCB->VTOR = FLASH_BASE | BMS_VECTOR_OFFSET;
    SystemCoreClockUpdate();
}
