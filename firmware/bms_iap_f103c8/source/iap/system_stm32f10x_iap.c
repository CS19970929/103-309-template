#include "stm32f10x.h"

#define SYSCLK_SOURCE_HSE               0x04u
#define SYSCLK_SOURCE_HSI               0x00u
#define HSE_STARTUP_TIMEOUT_LOOPS       0x0500u

uint32_t SystemCoreClock = HSE_VALUE;
__I uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};

static void select_hse_or_hsi(void)
{
    uint32_t wait = HSE_STARTUP_TIMEOUT_LOOPS;

    RCC->CR |= RCC_CR_HSEON;
    while (((RCC->CR & RCC_CR_HSERDY) == 0u) && (wait != 0u))
    {
        wait--;
    }

    if ((RCC->CR & RCC_CR_HSERDY) != 0u)
    {
        RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_SW);
        RCC->CFGR |= RCC_CFGR_SW_HSE;
        while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE)
        {
        }
        SystemCoreClock = HSE_VALUE;
    }
    else
    {
        RCC->CR |= RCC_CR_HSION;
        while ((RCC->CR & RCC_CR_HSIRDY) == 0u)
        {
        }
        RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_SW);
        RCC->CFGR |= RCC_CFGR_SW_HSI;
        while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI)
        {
        }
        SystemCoreClock = HSI_VALUE;
    }
}

void SystemInit(void)
{
    RCC->CR |= RCC_CR_HSION;
    RCC->CFGR &= 0xF8FF0000u;
    RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON);
    RCC->CR &= ~RCC_CR_HSEBYP;
    RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL);
    RCC->CIR = 0x009F0000u;

#ifdef RCC_CFGR2_PREDIV1
    RCC->CFGR2 = 0u;
#endif

    SCB->VTOR = FLASH_BASE;
    select_hse_or_hsi();
}

void SystemCoreClockUpdate(void)
{
    uint32_t sws = RCC->CFGR & RCC_CFGR_SWS;

    if (sws == RCC_CFGR_SWS_HSE)
    {
        SystemCoreClock = HSE_VALUE;
    }
    else
    {
        SystemCoreClock = HSI_VALUE;
    }
}
