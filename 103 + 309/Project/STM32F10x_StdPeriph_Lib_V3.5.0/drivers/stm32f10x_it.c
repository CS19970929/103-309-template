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
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
 * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
 * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
 * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "main.h" //在it的头文件可能会导致别的地方也能调用main的东西，不符合安全规范

// #include "stm32_eval.h"

/** @addtogroup STM32F10x_StdPeriph_Examples
 * @{
 */

/** @addtogroup EXTI_Config
 * @{
 */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
 * @brief  This function handles NMI exception.
 * @param  None
 * @retval None
 */
void NMI_Handler(void)
{
}

#define HARDFAULT_SRAM_START   0x20000000UL
#define HARDFAULT_SRAM_END     0x20005000UL

volatile HardFaultRecord g_hardfault_record;

void HardFault_Record(uint32_t *stack_pointer, uint32_t exc_return)
{
  uint32_t stack_address = (uint32_t)stack_pointer;

  g_hardfault_record.magic = 0x48465231UL;
  g_hardfault_record.count++;
  g_hardfault_record.stack_pointer = stack_address;
  g_hardfault_record.exc_return = exc_return;
  if (stack_address >= HARDFAULT_SRAM_START &&
      stack_address <= HARDFAULT_SRAM_END - (8U * sizeof(uint32_t)))
  {
    g_hardfault_record.r0 = stack_pointer[0];
    g_hardfault_record.r1 = stack_pointer[1];
    g_hardfault_record.r2 = stack_pointer[2];
    g_hardfault_record.r3 = stack_pointer[3];
    g_hardfault_record.r12 = stack_pointer[4];
    g_hardfault_record.lr = stack_pointer[5];
    g_hardfault_record.pc = stack_pointer[6];
    g_hardfault_record.xpsr = stack_pointer[7];
  }
  g_hardfault_record.cfsr = SCB->CFSR;
  g_hardfault_record.hfsr = SCB->HFSR;
  g_hardfault_record.dfsr = SCB->DFSR;
  g_hardfault_record.mmfar = SCB->MMFAR;
  g_hardfault_record.bfar = SCB->BFAR;
  g_hardfault_record.afsr = SCB->AFSR;
  g_hardfault_record.shcsr = SCB->SHCSR;
  g_hardfault_record.icsr = SCB->ICSR;
  g_hardfault_record.wake_reason = (uint32_t)g_irq_t;
  g_hardfault_record.usart1_sr = USART1->SR;
  g_hardfault_record.usart2_sr = USART2->SR;
  g_hardfault_record.exti_pr = EXTI->PR;
  g_hardfault_record.rtc_crl = RTC->CRL;

  while (1)
  {
    __NOP();
  }
}

__asm void HardFault_Handler(void)
{
  IMPORT HardFault_Record
  TST LR, #4
  ITE EQ
  MRSEQ R0, MSP
  MRSNE R0, PSP
  MOV R1, LR
  B HardFault_Record
}

/**
 * @brief  This function handles Memory Manage exception.
 * @param  None
 * @retval None
 */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
 * @brief  This function handles Bus Fault exception.
 * @param  None
 * @retval None
 */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
 * @brief  This function handles Usage Fault exception.
 * @param  None
 * @retval None
 */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
 * @brief  This function handles SVCall exception.
 * @param  None
 * @retval None
 */
void SVC_Handler(void)
{
}

/**
 * @brief  This function handles Debug Monitor exception.
 * @param  None
 * @retval None
 */
void DebugMon_Handler(void)
{
}

/**
 * @brief  This function handles PendSV_Handler exception.
 * @param  None
 * @retval None
 */
void PendSV_Handler(void)
{
}

/**
 * @brief  This function handles SysTick Handler.
 * @param  None
 * @retval None
 */
void SysTick_Handler(void)
{
}

/******************************************************************************/
/*            STM32F10x Peripherals Interrupt Handlers                        */
/******************************************************************************/

/**
 * @brief  This function handles External line 0 interrupt request.
 * @param  None
 * @retval Nonehttps://www.cingta.com/static/image/20190816/bb94f7d7c1f94c4daf38e66b8aaa80cd.png
 */
void EXTI0_IRQHandler(void)
{
  if (EXTI_GetITStatus(EXTI_Line0) != RESET)
  {
    EXTI_ClearITPendingBit(EXTI_Line0);
    rtc_sleep_record_wakeup(PA0_irq);
    ChargerLoad_Func.bits.b1ON_Charger_AllSeries = 1;
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
    rtc_sleep_record_wakeup(uart2_irq);
  }
}

void EXTI15_10_IRQHandler(void)
{
  if (EXTI_GetITStatus(EXTI_Line12) != RESET)
  {
    EXTI_ClearITPendingBit(EXTI_Line12);
    rtc_sleep_record_wakeup(rs485_irq);
  }
}

/**
 * @brief  This function handles External lines 9 to 5 interrupt request.
 * @param  None
 * @retval None
 */
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
    EXTI_ClearITPendingBit(EXTI_Line7);
    rtc_sleep_record_wakeup(uart1_irq);
  }
  if (EXTI_GetITStatus(EXTI_Line9) != RESET)
  {
    EXTI_ClearITPendingBit(EXTI_Line9);
    rtc_sleep_record_wakeup(bms_keyirq);
  }
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/

/**
 * @brief  This function handles PPP interrupt request.
 * @param  None
 * @retval None
 */
/*void PPP_IRQHandler(void)
{
}*/

/**
 * @}
 */

/**
 * @}
 */

// 以下的是非共有的，如果多处使用到，则移动到it.c文件
void USART1_IRQHandler(void)
{
#ifdef _COMMOM_UPPER_SCI1
  if (Sci1_CommonUpper_IRQHandler(&g_stCurrentMsgPtr_SCI1))
  {
    RTC_ExtComCnt++;
    rtc_sleep_record_wakeup(uart1_irq);
  }
#endif
}

void USART2_IRQHandler(void)
{
#ifdef _COMMOM_UPPER_SCI2
  if (Sci2_CommonUpper_IRQHandler(&g_stCurrentMsgPtr_SCI2))
  {
    RTC_ExtComCnt++;
    rtc_sleep_record_wakeup(uart2_irq);
  }
#endif
}

void USART3_IRQHandler(void)
{
#ifdef _COMMOM_UPPER_SCI3
  Sci3_CommonUpper_FaultChk();
  if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
  {
    RTC_ExtComCnt++;
    rtc_sleep_record_wakeup(uart3_irq);
    Sci3_CommonUpper_Rx_Deal(&g_stCurrentMsgPtr_SCI3);
  }
#endif
}
/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
