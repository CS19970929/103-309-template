# 04 中断与异步入口流程

状态：已按启动向量表和C ISR定义梳理。参考源码：`startup_stm32f10x_hd.s`、`stm32f10x_it.c`、`System_Init.c`、`LedBar.c`、`Can_HDX.c`、`RTC.c`、`Sci_Upper.c`。

## 目录
- [向量表关系](#向量表关系)
- [中断明细](#中断明细)
- [主循环共享变量与竞态](#主循环共享变量与竞态)
- [疑似默认弱处理向量](#疑似默认弱处理向量)

## 向量表关系

| 向量/符号 | 注释 | 向量文件 | 行 | 当前定义 |
| --- | --- | --- | --- | --- |
| __initial_sp | Top of Stack | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 62 | 弱默认/未在C中定义 |
| Reset_Handler | Reset Handler | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 63 | 弱默认/未在C中定义 |
| NMI_Handler | NMI Handler | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 64 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:29 |
| HardFault_Handler | Hard Fault Handler | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 65 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:89 |
| MemManage_Handler | MPU Fault Handler | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 66 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:101 |
| BusFault_Handler | Bus Fault Handler | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 67 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:113 |
| UsageFault_Handler | Usage Fault Handler | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 68 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:125 |
| SVC_Handler | SVCall Handler | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 73 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:137 |
| DebugMon_Handler | Debug Monitor Handler | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 74 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:147 |
| PendSV_Handler | PendSV Handler | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 76 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:157 |
| SysTick_Handler | SysTick Handler | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 77 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:167 |
| WWDG_IRQHandler | Window Watchdog | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 80 | 弱默认/未在C中定义 |
| PVD_IRQHandler | PVD through EXTI Line detect | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 81 | 弱默认/未在C中定义 |
| TAMPER_IRQHandler | Tamper | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 82 | 弱默认/未在C中定义 |
| RTC_IRQHandler | RTC | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 83 | 103 + 309/Project/Source/RTC.c:567 |
| FLASH_IRQHandler | Flash | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 84 | 弱默认/未在C中定义 |
| RCC_IRQHandler | RCC | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 85 | 弱默认/未在C中定义 |
| EXTI0_IRQHandler | EXTI Line 0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 86 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:177 |
| EXTI1_IRQHandler | EXTI Line 1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 87 | 弱默认/未在C中定义 |
| EXTI2_IRQHandler | EXTI Line 2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 88 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:203 |
| EXTI3_IRQHandler | EXTI Line 3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 89 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:219 |
| EXTI4_IRQHandler | EXTI Line 4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 90 | 弱默认/未在C中定义 |
| DMA1_Channel1_IRQHandler | DMA1 Channel 1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 91 | 弱默认/未在C中定义 |
| DMA1_Channel2_IRQHandler | DMA1 Channel 2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 92 | 弱默认/未在C中定义 |
| DMA1_Channel3_IRQHandler | DMA1 Channel 3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 93 | 弱默认/未在C中定义 |
| DMA1_Channel4_IRQHandler | DMA1 Channel 4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 94 | 弱默认/未在C中定义 |
| DMA1_Channel5_IRQHandler | DMA1 Channel 5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 95 | 弱默认/未在C中定义 |
| DMA1_Channel6_IRQHandler | DMA1 Channel 6 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 96 | 弱默认/未在C中定义 |
| DMA1_Channel7_IRQHandler | DMA1 Channel 7 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 97 | 弱默认/未在C中定义 |
| ADC1_2_IRQHandler | ADC1 & ADC2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 98 | 弱默认/未在C中定义 |
| USB_HP_CAN1_TX_IRQHandler | USB High Priority or CAN1 TX | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 99 | 弱默认/未在C中定义 |
| USB_LP_CAN1_RX0_IRQHandler | USB Low Priority or CAN1 RX0 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 100 | 103 + 309/Project/Source/Can_HDX.c:949 |
| CAN1_RX1_IRQHandler | CAN1 RX1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 101 | 弱默认/未在C中定义 |
| CAN1_SCE_IRQHandler | CAN1 SCE | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 102 | 弱默认/未在C中定义 |
| EXTI9_5_IRQHandler | EXTI Line 9..5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 103 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:257 |
| TIM1_BRK_IRQHandler | TIM1 Break | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 104 | 弱默认/未在C中定义 |
| TIM1_UP_IRQHandler | TIM1 Update | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 105 | 弱默认/未在C中定义 |
| TIM1_TRG_COM_IRQHandler | TIM1 Trigger and Commutation | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 106 | 弱默认/未在C中定义 |
| TIM1_CC_IRQHandler | TIM1 Capture Compare | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 107 | 弱默认/未在C中定义 |
| TIM2_IRQHandler | TIM2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 108 | 弱默认/未在C中定义 |
| TIM3_IRQHandler | TIM3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 109 | 103 + 309/Project/Source/System_Init.c:320 |
| TIM4_IRQHandler | TIM4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 110 | 103 + 309/Project/Source/LedBar.c:1306 |
| I2C1_EV_IRQHandler | I2C1 Event | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 111 | 弱默认/未在C中定义 |
| I2C1_ER_IRQHandler | I2C1 Error | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 112 | 弱默认/未在C中定义 |
| I2C2_EV_IRQHandler | I2C2 Event | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 113 | 弱默认/未在C中定义 |
| I2C2_ER_IRQHandler | I2C2 Error | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 114 | 弱默认/未在C中定义 |
| SPI1_IRQHandler | SPI1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 115 | 弱默认/未在C中定义 |
| SPI2_IRQHandler | SPI2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 116 | 弱默认/未在C中定义 |
| USART1_IRQHandler | USART1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 117 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:298 |
| USART2_IRQHandler | USART2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 118 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:332 |
| USART3_IRQHandler | USART3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 119 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:341 |
| EXTI15_10_IRQHandler | EXTI Line 15..10 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 120 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c:235 |
| RTCAlarm_IRQHandler | RTC Alarm through EXTI Line | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 121 | 103 + 309/Project/Source/RTC.c:561 |
| USBWakeUp_IRQHandler | USB Wakeup from suspend | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 122 | 弱默认/未在C中定义 |
| TIM8_BRK_IRQHandler | TIM8 Break | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 123 | 弱默认/未在C中定义 |
| TIM8_UP_IRQHandler | TIM8 Update | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 124 | 弱默认/未在C中定义 |
| TIM8_TRG_COM_IRQHandler | TIM8 Trigger and Commutation | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 125 | 弱默认/未在C中定义 |
| TIM8_CC_IRQHandler | TIM8 Capture Compare | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 126 | 弱默认/未在C中定义 |
| ADC3_IRQHandler | ADC3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 127 | 弱默认/未在C中定义 |
| FSMC_IRQHandler | FSMC | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 128 | 弱默认/未在C中定义 |
| SDIO_IRQHandler | SDIO | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 129 | 弱默认/未在C中定义 |
| TIM5_IRQHandler | TIM5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 130 | 弱默认/未在C中定义 |
| SPI3_IRQHandler | SPI3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 131 | 弱默认/未在C中定义 |
| UART4_IRQHandler | UART4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 132 | 弱默认/未在C中定义 |
| UART5_IRQHandler | UART5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 133 | 弱默认/未在C中定义 |
| TIM6_IRQHandler | TIM6 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 134 | 弱默认/未在C中定义 |
| TIM7_IRQHandler | TIM7 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 135 | 弱默认/未在C中定义 |
| DMA2_Channel1_IRQHandler | DMA2 Channel1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 136 | 弱默认/未在C中定义 |
| DMA2_Channel2_IRQHandler | DMA2 Channel2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 137 | 弱默认/未在C中定义 |
| DMA2_Channel3_IRQHandler | DMA2 Channel3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 138 | 弱默认/未在C中定义 |
| DMA2_Channel4_5_IRQHandler | DMA2 Channel4 & Channel5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 139 | 弱默认/未在C中定义 |


## 中断明细

| 中断函数 | 触发来源 | 文件 | 处理内容 | 写入/修改变量 | 共享/竞态说明 | 风险 |
| --- | --- | --- | --- | --- | --- | --- |
| NMI_Handler | NMI异常 | stm32f10x_it.c | IrqDebug_Count(IRQDBG_NMI) | 仅计数 | 无业务处理 | 低 |
| HardFault_Handler | 硬Fault | stm32f10x_it.c | IrqDebug计数/阶段，BKP保存FAULT_REASON_HARD，非_DEBUG_复位 | BKP fault snapshot | 会复位 | 高：异常恢复路径 |
| MemManage_Handler | MPU fault | stm32f10x_it.c | 同HardFault，reason=MEM | BKP fault snapshot | 会复位 | 高 |
| BusFault_Handler | Bus fault | stm32f10x_it.c | 同HardFault，reason=BUS | BKP fault snapshot | 会复位 | 高 |
| UsageFault_Handler | Usage fault | stm32f10x_it.c | 同HardFault，reason=USAGE | BKP fault snapshot | 会复位 | 高 |
| SysTick_Handler | SysTick | stm32f10x_it.c | 仅IrqDebug_CountFast；系统节拍不靠SysTick ISR | IrqDebug | 无业务 | 低 |
| TIM3_IRQHandler | TIM3 update 10ms | System_Init.c | 清TIM3 pending，调用SysTime_Post10msTick | s_st_SysTimePending, s_u32Sys10msTickCount, s_u8Cnt* | 主循环共享，变量volatile/临界区锁存 | 高：所有周期任务基准 |
| TIM4_IRQHandler | TIM4 update 1ms LED扫描 | LedBar.c | 清TIM4 pending，LedBar_Scan1ms | s_ledbar | 和APP_LedBar共享帧/scan_index | 中：LED显示一致性 |
| USART1_IRQHandler | USART1 RX/TX/IDLE等 | stm32f10x_it.c -> Sci_Upper.c | 计数后调用Sci1_CommonUpper_IRQHandler | g_stSciPort1, g_stCurrentMsgPtr_SCI1, tx flags | 主循环App_CommonUpper共享SCI状态 | 高：Modbus通信 |
| USART2_IRQHandler | USART2 | stm32f10x_it.c | 仅_COMMOM_UPPER_SCI2定义时调用SCI2；当前未见启用 | SCI2状态（条件编译） | 当前疑似未启用 | 低/需确认 |
| USART3_IRQHandler | USART3 | stm32f10x_it.c | 仅_COMMOM_UPPER_SCI3定义时编译；当前未见启用 | SCI3状态（条件编译） | 当前疑似未启用 | 低/需确认 |
| USB_LP_CAN1_RX0_IRQHandler | CAN1 RX FIFO0 | Can_HDX.c | 循环接收FIFO0，sys_time.can_rcv_cnt++，APP命令入队 | s_app.cmd_queue, sys_time.can_rcv_cnt | 主循环App_Can取队列；cmd_head/tail/count volatile且取队列关中断 | 高：CAN命令入口 |
| EXTI0_IRQHandler | PA0 CHG_IN | stm32f10x_it.c | 计数并清pending | sys_time.cnt_PA0_irq | 用于唤醒/诊断 | 中：充电唤醒 |
| EXTI9_5_IRQHandler | EXTI5/6/7/9；SW key/UART1 wake | stm32f10x_it.c | EXTI9设g_irq_t=soc_key，EXTI7仅计数 | g_irq_t, sys_time.cnt_bms1_keyirq | 低功耗唤醒源共享 | 中 |
| EXTI15_10_IRQHandler | EXTI12 CMNT wake / EXTI13 MCU wake | stm32f10x_it.c | 计数清pending | IrqDebug | 低功耗唤醒源 | 中 |
| RTC_IRQHandler | RTC秒/闹钟 | RTC.c | 秒中断设置s_rtc.disp并计数；闹钟设置s_rtc.wake | s_rtc, sys_time.rtc_sec_cnt/rtc_alm_cnt | rtc_sleep读s_rtc.wake | 高：RTC STOP唤醒/时间 |
| RTCAlarm_IRQHandler | RTC Alarm EXTI17 | RTC.c | 调用RTC_HandleAlarmWakeup | s_rtc.wake, sys_time.rtc_alm_cnt | rtc_sleep唤醒判断 | 高 |


## 主循环共享变量与竞态

- `s_st_SysTimePending`、`s_u32Sys10msTickCount`、`s_u8Sys200msPendingPeriods` 在 `TIM3_IRQHandler()` 写，在主循环通过关中断临界区读取/消费，声明为 `volatile`，当前边界清晰。
- `s_app.cmd_head/cmd_tail/cmd_count` 在 CAN ISR 入队、主循环出队；结构字段声明为 `volatile`，出队和清队列关中断，入队在ISR上下文不再嵌套同级CAN中断，风险可控。
- SCI端口状态在USART ISR和主循环共享，关键flag为 `volatile` 指针字段；仍建议后续重构时保持“ISR只推进字节和TX，主循环解析业务”的边界。
- `s_ledbar` 同时被 `TIM4_IRQHandler()` 扫描和 `APP_LedBar()` 更新；当前未见统一临界区，依赖字段写入较短和显示容错。重构LED时需重点检查帧切换原子性。
- `s_rtc.wake/disp` 在RTC ISR写、主循环/低功耗读取；`s_rtc` 的定义需结合 `RTC.c` 继续确认 volatile 粒度，当前从行为上属于共享状态。



## 疑似默认弱处理向量

这些向量在启动文件中存在，但本次未找到强C定义；触发后可能进入 `Default_Handler` 并记录未处理向量，或停在弱默认。需结合链接结果确认。

| 向量 | 注释 | 文件 | 行 | 定义状态 |
| --- | --- | --- | --- | --- |
| __initial_sp | Top of Stack | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 62 | 弱默认/未在C中定义 |
| Reset_Handler | Reset Handler | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 63 | 弱默认/未在C中定义 |
| WWDG_IRQHandler | Window Watchdog | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 80 | 弱默认/未在C中定义 |
| PVD_IRQHandler | PVD through EXTI Line detect | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 81 | 弱默认/未在C中定义 |
| TAMPER_IRQHandler | Tamper | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 82 | 弱默认/未在C中定义 |
| FLASH_IRQHandler | Flash | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 84 | 弱默认/未在C中定义 |
| RCC_IRQHandler | RCC | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 85 | 弱默认/未在C中定义 |
| EXTI1_IRQHandler | EXTI Line 1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 87 | 弱默认/未在C中定义 |
| EXTI4_IRQHandler | EXTI Line 4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 90 | 弱默认/未在C中定义 |
| DMA1_Channel1_IRQHandler | DMA1 Channel 1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 91 | 弱默认/未在C中定义 |
| DMA1_Channel2_IRQHandler | DMA1 Channel 2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 92 | 弱默认/未在C中定义 |
| DMA1_Channel3_IRQHandler | DMA1 Channel 3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 93 | 弱默认/未在C中定义 |
| DMA1_Channel4_IRQHandler | DMA1 Channel 4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 94 | 弱默认/未在C中定义 |
| DMA1_Channel5_IRQHandler | DMA1 Channel 5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 95 | 弱默认/未在C中定义 |
| DMA1_Channel6_IRQHandler | DMA1 Channel 6 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 96 | 弱默认/未在C中定义 |
| DMA1_Channel7_IRQHandler | DMA1 Channel 7 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 97 | 弱默认/未在C中定义 |
| ADC1_2_IRQHandler | ADC1 & ADC2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 98 | 弱默认/未在C中定义 |
| USB_HP_CAN1_TX_IRQHandler | USB High Priority or CAN1 TX | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 99 | 弱默认/未在C中定义 |
| CAN1_RX1_IRQHandler | CAN1 RX1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 101 | 弱默认/未在C中定义 |
| CAN1_SCE_IRQHandler | CAN1 SCE | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 102 | 弱默认/未在C中定义 |
| TIM1_BRK_IRQHandler | TIM1 Break | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 104 | 弱默认/未在C中定义 |
| TIM1_UP_IRQHandler | TIM1 Update | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 105 | 弱默认/未在C中定义 |
| TIM1_TRG_COM_IRQHandler | TIM1 Trigger and Commutation | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 106 | 弱默认/未在C中定义 |
| TIM1_CC_IRQHandler | TIM1 Capture Compare | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 107 | 弱默认/未在C中定义 |
| TIM2_IRQHandler | TIM2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 108 | 弱默认/未在C中定义 |
| I2C1_EV_IRQHandler | I2C1 Event | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 111 | 弱默认/未在C中定义 |
| I2C1_ER_IRQHandler | I2C1 Error | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 112 | 弱默认/未在C中定义 |
| I2C2_EV_IRQHandler | I2C2 Event | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 113 | 弱默认/未在C中定义 |
| I2C2_ER_IRQHandler | I2C2 Error | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 114 | 弱默认/未在C中定义 |
| SPI1_IRQHandler | SPI1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 115 | 弱默认/未在C中定义 |
| SPI2_IRQHandler | SPI2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 116 | 弱默认/未在C中定义 |
| USBWakeUp_IRQHandler | USB Wakeup from suspend | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 122 | 弱默认/未在C中定义 |
| TIM8_BRK_IRQHandler | TIM8 Break | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 123 | 弱默认/未在C中定义 |
| TIM8_UP_IRQHandler | TIM8 Update | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 124 | 弱默认/未在C中定义 |
| TIM8_TRG_COM_IRQHandler | TIM8 Trigger and Commutation | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 125 | 弱默认/未在C中定义 |
| TIM8_CC_IRQHandler | TIM8 Capture Compare | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 126 | 弱默认/未在C中定义 |
| ADC3_IRQHandler | ADC3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 127 | 弱默认/未在C中定义 |
| FSMC_IRQHandler | FSMC | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 128 | 弱默认/未在C中定义 |
| SDIO_IRQHandler | SDIO | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 129 | 弱默认/未在C中定义 |
| TIM5_IRQHandler | TIM5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 130 | 弱默认/未在C中定义 |
| SPI3_IRQHandler | SPI3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 131 | 弱默认/未在C中定义 |
| UART4_IRQHandler | UART4 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 132 | 弱默认/未在C中定义 |
| UART5_IRQHandler | UART5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 133 | 弱默认/未在C中定义 |
| TIM6_IRQHandler | TIM6 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 134 | 弱默认/未在C中定义 |
| TIM7_IRQHandler | TIM7 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 135 | 弱默认/未在C中定义 |
| DMA2_Channel1_IRQHandler | DMA2 Channel1 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 136 | 弱默认/未在C中定义 |
| DMA2_Channel2_IRQHandler | DMA2 Channel2 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 137 | 弱默认/未在C中定义 |
| DMA2_Channel3_IRQHandler | DMA2 Channel3 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 138 | 弱默认/未在C中定义 |
| DMA2_Channel4_5_IRQHandler | DMA2 Channel4 & Channel5 | 103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s | 139 | 弱默认/未在C中定义 |