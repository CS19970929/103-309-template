#include "stm32f10x.h"

#define SYSCLK_72MHZ 72000000u
#define HSI_VALUE_LOCAL 8000000u
#define HSE_VALUE_LOCAL 8000000u

uint32_t SystemCoreClock = SYSCLK_72MHZ;

static void SetSysClockTo72(void)
{
    volatile uint32_t startup_counter = 0u;
    uint32_t hse_ready = 0u;

    RCC->CR |= RCC_CR_HSEON;
    do {
        hse_ready = RCC->CR & RCC_CR_HSERDY;
        startup_counter++;
    } while ((hse_ready == 0u) && (startup_counter < 0x5000u));

    if (hse_ready != 0u) {
        FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

        RCC->CFGR |= RCC_CFGR_HPRE_DIV1;
        RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;
        RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

        RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL);
        RCC->CFGR |= RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;

        RCC->CR |= RCC_CR_PLLON;
        while ((RCC->CR & RCC_CR_PLLRDY) == 0u) {
        }

        RCC->CFGR &= ~RCC_CFGR_SW;
        RCC->CFGR |= RCC_CFGR_SW_PLL;
        while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
        }
        SystemCoreClock = SYSCLK_72MHZ;
    } else {
        SystemCoreClock = HSI_VALUE_LOCAL;
    }
}

void SystemInit(void)
{
    RCC->CR |= RCC_CR_HSION;
    RCC->CFGR = 0x00000000u;
    RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON);
    RCC->CR &= ~RCC_CR_HSEBYP;
    RCC->CFGR &= ~RCC_CFGR_PLLMULL;
    RCC->CFGR &= ~RCC_CFGR_PLLSRC;
    RCC->CIR = 0x009F0000u;

    SCB->VTOR = FLASH_BASE;
    SetSysClockTo72();
}

void SystemCoreClockUpdate(void)
{
    uint32_t sws = RCC->CFGR & RCC_CFGR_SWS;
    uint32_t pllmul;

    if (sws == RCC_CFGR_SWS_HSI) {
        SystemCoreClock = HSI_VALUE_LOCAL;
    } else if (sws == RCC_CFGR_SWS_HSE) {
        SystemCoreClock = HSE_VALUE_LOCAL;
    } else if (sws == RCC_CFGR_SWS_PLL) {
        pllmul = ((RCC->CFGR & RCC_CFGR_PLLMULL) >> 18u) + 2u;
        if ((RCC->CFGR & RCC_CFGR_PLLSRC) != 0u) {
            SystemCoreClock = HSE_VALUE_LOCAL * pllmul;
        } else {
            SystemCoreClock = (HSI_VALUE_LOCAL / 2u) * pllmul;
        }
    } else {
        SystemCoreClock = HSI_VALUE_LOCAL;
    }
}
