#include "board_uart.h"

#include "stm32f10x.h"

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    NVIC_SystemReset();
}

void MemManage_Handler(void)
{
    NVIC_SystemReset();
}

void BusFault_Handler(void)
{
    NVIC_SystemReset();
}

void UsageFault_Handler(void)
{
    NVIC_SystemReset();
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

void USART1_IRQHandler(void)
{
    BoardUart_IrqHandler();
}
