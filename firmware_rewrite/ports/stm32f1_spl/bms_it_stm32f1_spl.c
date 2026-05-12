#include "bms_port_stm32f1_spl.h"

#include "stm32f10x_exti.h"
#include "stm32f10x_rtc.h"

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    while (1) {
    }
}

void MemManage_Handler(void)
{
    while (1) {
    }
}

void BusFault_Handler(void)
{
    while (1) {
    }
}

void UsageFault_Handler(void)
{
    while (1) {
    }
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

void SysTick_Handler(void)
{
    bms_stm32f1_tick_isr();
}

void EXTI0_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}

void EXTI9_5_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line5) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line5);
    }
    if (EXTI_GetITStatus(EXTI_Line6) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line6);
    }
    if (EXTI_GetITStatus(EXTI_Line7) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line7);
    }
    if (EXTI_GetITStatus(EXTI_Line9) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line9);
    }
}

void EXTI15_10_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line12) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line12);
    }
    if (EXTI_GetITStatus(EXTI_Line13) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line13);
    }
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
    bms_stm32f1_can_rx_isr();
}

void CAN1_RX1_IRQHandler(void)
{
    bms_stm32f1_can_rx_isr();
}

void USART1_IRQHandler(void)
{
    bms_stm32f1_usart_rx_isr();
}

void USART2_IRQHandler(void)
{
    bms_stm32f1_usart_rx_isr();
}

void USART3_IRQHandler(void)
{
    bms_stm32f1_usart_rx_isr();
}

void RTCAlarm_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line17) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line17);
    }
    if (RTC_GetFlagStatus(RTC_FLAG_ALR) != RESET) {
        RTC_ClearITPendingBit(RTC_IT_ALR);
        RTC_ClearFlag(RTC_FLAG_ALR);
        RTC_WaitForLastTask();
    }
}
