/**
 ******************************************************************************
 * @file    EXTI/EXTI_Config/stm32f10x_it.c
 * @author  MCD Application Team
 * @version V3.5.0
 * @date    08-April-2011
 * @brief   Main Interrupt Service Routines.
 *          This file provides template for all exceptions handler and peripherals
 *          interrupt service routine.
 ******************************************************************************
 * @attention
 *
 * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION IN ORDER FOR THEM TO SAVE TIME. AS A RESULT,
 * STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT, INDIRECT OR
 * CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIM ARISING FROM THE CONTENT OF
 * THIS FIRMWARE OR ITS USE.
 *
 * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
 ******************************************************************************
 */

#include "stm32f10x_it.h"
#include "main.h"
#include "FaultSnapshot.h"

void NMI_Handler(void)
{
}

__asm void wait()
{
  BX lr
}

static void Fault_SaveReason(UINT16 reason)
{
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
  PWR_BackupAccessCmd(ENABLE);
  BKP_WriteBackupRegister(FAULT_BKP_REASON_REG, reason);
  BKP_WriteBackupRegister(FAULT_BKP_REASON_INV_REG, (UINT16)(~reason));
}

static void Fault_ResetOrHold(UINT16 reason)
{
  Fault_SaveReason(reason);
#ifdef _DEBUG_
  while (1)
  {
    wait();
  }
#else
  NVIC_SystemReset();
  while (1)
  {
  }
#endif
}

void HardFault_Handler(void)
{
  Fault_ResetOrHold(FAULT_REASON_HARD);
}

void MemManage_Handler(void)
{
  Fault_ResetOrHold(FAULT_REASON_MEM);
}

void BusFault_Handler(void)
{
  Fault_ResetOrHold(FAULT_REASON_BUS);
}

void UsageFault_Handler(void)
{
  Fault_ResetOrHold(FAULT_REASON_USAGE);
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
}

void EXTI0_IRQHandler(void)
{
  if (EXTI_GetITStatus(EXTI_Line0) != RESET)
  {
    sys_time.cnt_PA0_irq++;
    g_irq_t = PA0_irq;
    EXTI_ClearITPendingBit(EXTI_Line0);
  }
}

void EXTI2_IRQHandler(void)
{
  if (EXTI_GetITStatus(EXTI_Line2) != RESET)
  {
    EXTI_ClearITPendingBit(EXTI_Line2);
  }
}

void EXTI3_IRQHandler(void)
{
  if (EXTI_GetITStatus(EXTI_Line3) != RESET)
  {
    EXTI_ClearITPendingBit(EXTI_Line3);
  }
}

void EXTI15_10_IRQHandler(void)
{
  if (EXTI_GetITStatus(EXTI_Line12) != RESET)
  {
    g_irq_t = can_wake_irq;
    EXTI_ClearITPendingBit(EXTI_Line12);
  }

  if (EXTI_GetITStatus(EXTI_Line13) != RESET)
  {
    g_irq_t = rs485_irq;
    EXTI_ClearITPendingBit(EXTI_Line13);
  }
}

void EXTI9_5_IRQHandler(void)
{
  if (EXTI_GetITStatus(EXTI_Line5) != RESET)
  {
    EXTI_ClearITPendingBit(EXTI_Line5);
  }

  if (EXTI_GetITStatus(EXTI_Line6) != RESET)
  {
    EXTI_ClearITPendingBit(EXTI_Line6);
  }

  if (EXTI_GetITStatus(EXTI_Line7) != RESET)
  {
    g_irq_t = uart1_irq;
    EXTI_ClearITPendingBit(EXTI_Line7);
  }

  if (EXTI_GetITStatus(EXTI_Line9) != RESET)
  {
    sys_time.cnt_bms1_keyirq++;
    g_irq_t = soc_key;
    EXTI_ClearITPendingBit(EXTI_Line9);
  }
}

void USART1_IRQHandler(void)
{
  sys_time.sci1_irq_cnt++;
#ifdef _COMMOM_UPPER_SCI1
  Sci1_CommonUpper_IRQHandler();
#endif
}

void USART2_IRQHandler(void)
{
  sys_time.sci2_irq_cnt++;
#ifdef _COMMOM_UPPER_SCI2
  Sci2_CommonUpper_IRQHandler();
#endif
}

#ifdef _COMMOM_UPPER_SCI3
void USART3_IRQHandler(void)
{
  sys_time.sci3_irq_cnt++;
  Sci3_CommonUpper_IRQHandler();
}
#endif

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
