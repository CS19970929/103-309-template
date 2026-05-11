#include "stm32f10x.h"

static void Iap_ResetOnFault(void)
{
    NVIC_SystemReset();
    while (1)
    {
    }
}

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    Iap_ResetOnFault();
}

void MemManage_Handler(void)
{
    Iap_ResetOnFault();
}

void BusFault_Handler(void)
{
    Iap_ResetOnFault();
}

void UsageFault_Handler(void)
{
    Iap_ResetOnFault();
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void USB_HP_CAN1_TX_IRQHandler(void)
{
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
}

void CAN1_RX1_IRQHandler(void)
{
}

void CAN1_SCE_IRQHandler(void)
{
}
