.syntax unified
.cpu cortex-m3
.thumb

.global __Vectors
.global __Vectors_End
.global __Vectors_Size
.global g_pfnVectors

.section .isr_vector, "a", %progbits
.type __Vectors, %object
__Vectors:
g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler
  .word WWDG_IRQHandler
  .word PVD_IRQHandler
  .word TAMPER_IRQHandler
  .word RTC_IRQHandler
  .word FLASH_IRQHandler
  .word RCC_IRQHandler
  .word EXTI0_IRQHandler
  .word EXTI1_IRQHandler
  .word EXTI2_IRQHandler
  .word EXTI3_IRQHandler
  .word EXTI4_IRQHandler
  .word DMA1_Channel1_IRQHandler
  .word DMA1_Channel2_IRQHandler
  .word DMA1_Channel3_IRQHandler
  .word DMA1_Channel4_IRQHandler
  .word DMA1_Channel5_IRQHandler
  .word DMA1_Channel6_IRQHandler
  .word DMA1_Channel7_IRQHandler
  .word ADC1_2_IRQHandler
  .word USB_HP_CAN1_TX_IRQHandler
  .word USB_LP_CAN1_RX0_IRQHandler
  .word CAN1_RX1_IRQHandler
  .word CAN1_SCE_IRQHandler
  .word EXTI9_5_IRQHandler
  .word TIM1_BRK_IRQHandler
  .word TIM1_UP_IRQHandler
  .word TIM1_TRG_COM_IRQHandler
  .word TIM1_CC_IRQHandler
  .word TIM2_IRQHandler
  .word TIM3_IRQHandler
  .word TIM4_IRQHandler
  .word I2C1_EV_IRQHandler
  .word I2C1_ER_IRQHandler
  .word I2C2_EV_IRQHandler
  .word I2C2_ER_IRQHandler
  .word SPI1_IRQHandler
  .word SPI2_IRQHandler
  .word USART1_IRQHandler
  .word USART2_IRQHandler
  .word USART3_IRQHandler
  .word EXTI15_10_IRQHandler
  .word RTCAlarm_IRQHandler
  .word USBWakeUp_IRQHandler
  .word TIM8_BRK_IRQHandler
  .word TIM8_UP_IRQHandler
  .word TIM8_TRG_COM_IRQHandler
  .word TIM8_CC_IRQHandler
  .word ADC3_IRQHandler
  .word FSMC_IRQHandler
  .word SDIO_IRQHandler
  .word TIM5_IRQHandler
  .word SPI3_IRQHandler
  .word UART4_IRQHandler
  .word UART5_IRQHandler
  .word TIM6_IRQHandler
  .word TIM7_IRQHandler
  .word DMA2_Channel1_IRQHandler
  .word DMA2_Channel2_IRQHandler
  .word DMA2_Channel3_IRQHandler
  .word DMA2_Channel4_5_IRQHandler
__Vectors_End:
__Vectors_Size = __Vectors_End - __Vectors

.section .text.Reset_Handler, "ax", %progbits
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
  ldr r0, =_estack
  mov sp, r0

  bl SystemInit

  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
copy_data:
  cmp r0, r1
  bcc copy_data_word
  b zero_bss_start
copy_data_word:
  ldr r3, [r2], #4
  str r3, [r0], #4
  b copy_data

zero_bss_start:
  ldr r0, =_sbss
  ldr r1, =_ebss
  movs r2, #0
zero_bss:
  cmp r0, r1
  bcc zero_bss_word
  b call_main
zero_bss_word:
  str r2, [r0], #4
  b zero_bss

call_main:
  bl __libc_init_array
  bl main
loop_forever:
  b loop_forever
.size Reset_Handler, . - Reset_Handler

.section .text.Default_Handler, "ax", %progbits
.type Default_Handler, %function
Default_Handler:
  b Default_Handler
.size Default_Handler, . - Default_Handler

.macro def_irq name
  .weak \name
  .thumb_set \name, Default_Handler
.endm

def_irq NMI_Handler
def_irq HardFault_Handler
def_irq MemManage_Handler
def_irq BusFault_Handler
def_irq UsageFault_Handler
def_irq SVC_Handler
def_irq DebugMon_Handler
def_irq PendSV_Handler
def_irq SysTick_Handler
def_irq WWDG_IRQHandler
def_irq PVD_IRQHandler
def_irq TAMPER_IRQHandler
def_irq RTC_IRQHandler
def_irq FLASH_IRQHandler
def_irq RCC_IRQHandler
def_irq EXTI0_IRQHandler
def_irq EXTI1_IRQHandler
def_irq EXTI2_IRQHandler
def_irq EXTI3_IRQHandler
def_irq EXTI4_IRQHandler
def_irq DMA1_Channel1_IRQHandler
def_irq DMA1_Channel2_IRQHandler
def_irq DMA1_Channel3_IRQHandler
def_irq DMA1_Channel4_IRQHandler
def_irq DMA1_Channel5_IRQHandler
def_irq DMA1_Channel6_IRQHandler
def_irq DMA1_Channel7_IRQHandler
def_irq ADC1_2_IRQHandler
def_irq USB_HP_CAN1_TX_IRQHandler
def_irq USB_LP_CAN1_RX0_IRQHandler
def_irq CAN1_RX1_IRQHandler
def_irq CAN1_SCE_IRQHandler
def_irq EXTI9_5_IRQHandler
def_irq TIM1_BRK_IRQHandler
def_irq TIM1_UP_IRQHandler
def_irq TIM1_TRG_COM_IRQHandler
def_irq TIM1_CC_IRQHandler
def_irq TIM2_IRQHandler
def_irq TIM3_IRQHandler
def_irq TIM4_IRQHandler
def_irq I2C1_EV_IRQHandler
def_irq I2C1_ER_IRQHandler
def_irq I2C2_EV_IRQHandler
def_irq I2C2_ER_IRQHandler
def_irq SPI1_IRQHandler
def_irq SPI2_IRQHandler
def_irq USART1_IRQHandler
def_irq USART2_IRQHandler
def_irq USART3_IRQHandler
def_irq EXTI15_10_IRQHandler
def_irq RTCAlarm_IRQHandler
def_irq USBWakeUp_IRQHandler
def_irq TIM8_BRK_IRQHandler
def_irq TIM8_UP_IRQHandler
def_irq TIM8_TRG_COM_IRQHandler
def_irq TIM8_CC_IRQHandler
def_irq ADC3_IRQHandler
def_irq FSMC_IRQHandler
def_irq SDIO_IRQHandler
def_irq TIM5_IRQHandler
def_irq SPI3_IRQHandler
def_irq UART4_IRQHandler
def_irq UART5_IRQHandler
def_irq TIM6_IRQHandler
def_irq TIM7_IRQHandler
def_irq DMA2_Channel1_IRQHandler
def_irq DMA2_Channel2_IRQHandler
def_irq DMA2_Channel3_IRQHandler
def_irq DMA2_Channel4_5_IRQHandler
