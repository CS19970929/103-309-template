#include "iap_can_upgrade.h"
#include "iap_config.h"
#include "iap_flash.h"

#include "stm32f10x.h"

volatile uint32_t g_iap_tick_ms = 0U;

uint32_t Iap_GetTickMs(void)
{
    return g_iap_tick_ms;
}

void SysTick_Handler(void)
{
    g_iap_tick_ms++;
}

static void Iap_InitTick(void)
{
    SystemCoreClockUpdate();
    (void)SysTick_Config(SystemCoreClock / 1000U);
}

static uint8_t Iap_ShouldStayInBoot(void)
{
    if (IapFlash_ReadHalfWord(IAP_UPDATE_FLAG_ADDR) == IAP_FLASH_TO_IAP_VALUE)
    {
        return 1U;
    }
    if (IapFlash_IsAppVectorValid() == 0U)
    {
        return 1U;
    }
    return 0U;
}

int main(void)
{
    uint8_t stay_in_boot;
    uint32_t start_ms;

    Iap_InitTick();
    IapCan_Init();

    stay_in_boot = Iap_ShouldStayInBoot();
    if (stay_in_boot == 0U)
    {
        start_ms = Iap_GetTickMs();
        while ((uint32_t)(Iap_GetTickMs() - start_ms) < IAP_BOOT_WAIT_MS)
        {
            IapCan_Task();
            if (IapCan_IsUpgradeActive() != 0U)
            {
                stay_in_boot = 1U;
                break;
            }
        }
    }

    if ((stay_in_boot == 0U) && (Iap_ShouldStayInBoot() == 0U))
    {
        IapFlash_JumpToApp();
    }

    while (1)
    {
        IapCan_Task();
    }
}
