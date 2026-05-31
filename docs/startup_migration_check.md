# Startup 迁移检查

记录日期：2026-05-31

## 当前 startup 判断

| 检查项 | 结果 |
|---|---|
| 文件 | `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s` |
| 语法 | ARMCC/ARMASM |
| 证据 | 使用 `EQU`、`AREA`、`SPACE`、`PRESERVE8`、`DCD`、`EXPORT`、`IMPORT`、`PROC`、`ENDP` |
| Keil 行为 | `Reset_Handler` 调用 `SystemInit`，再跳转 `__main` |
| GCC 处理 | 新增 GCC 版本 startup，不覆盖旧文件 |

## GCC startup 必须满足

已新增 GCC startup：`startup/startup_stm32f10x_hd_gcc.s`。

| 检查项 | GCC startup 目标 | 状态 |
|---|---|---|
| 向量表地址 | `.isr_vector` 放在 `FLASH ORIGIN=0x08004800` | 已实现，Debug/Release 链接通过 |
| 初始栈 | 第一项为 `_estack` | 已实现，Debug/Release 链接通过 |
| `Reset_Handler` | 全局导出 | 已实现，Debug/Release 链接通过 |
| `SystemInit` | 复位后先调用 | 已实现，Debug/Release 链接通过 |
| `.data` 拷贝 | 从 `_sidata` 拷贝到 `_sdata.._edata` | 已实现，Debug/Release 链接通过 |
| `.bss` 清零 | 清 `_sbss.._ebss` | 已实现，Debug/Release 链接通过 |
| C runtime | 调用 `__libc_init_array` | 已实现，Debug/Release 链接通过 |
| `main` | 调用 `main`，返回后进入死循环 | 已实现，Debug/Release 链接通过 |
| 弱中断 | 未实现 ISR weak alias 到 `Default_Handler` | 已实现，Debug/Release 链接通过 |

## ISR 名称对照

源码中已实现的 ISR：

- `NMI_Handler`
- `HardFault_Handler`
- `MemManage_Handler`
- `BusFault_Handler`
- `UsageFault_Handler`
- `SVC_Handler`
- `DebugMon_Handler`
- `PendSV_Handler`
- `SysTick_Handler`
- `EXTI0_IRQHandler`
- `EXTI2_IRQHandler`
- `EXTI3_IRQHandler`
- `EXTI15_10_IRQHandler`
- `EXTI9_5_IRQHandler`
- `USART1_IRQHandler`
- `USART2_IRQHandler`
- `USART3_IRQHandler`，受 `_COMMOM_UPPER_SCI3` 条件影响
- `USB_LP_CAN1_RX0_IRQHandler`
- `TIM4_IRQHandler`
- `RTCAlarm_IRQHandler`
- `RTC_IRQHandler`
- `TIM3_IRQHandler`

GCC startup 的向量表名称必须与以上名称完全一致，未实现的中断保留 weak default。

## 迁移风险

- 高风险：当前 Keil 设备宏为 `STM32F10X_MD`，但 startup 文件是 `startup_stm32f10x_hd.s`。这是历史工程现状，GCC 先按原文件向量表迁移，后续必须结合实际芯片确认是否应换成 MD startup。
- 已处理：`stm32f10x_it.c` 中的 `__asm void wait()` 已增加 GCC 条件编译实现，ARMCC 原实现不变。
- 中风险：`SystemInit` 是否设置 `SCB->VTOR = FLASH_BASE | 0x4800` 依赖 `_IAP`，而 `_IAP` 由 `conf.h` 经 `main.h` 间接进入 `system_stm32f10x.c`。GCC include 路径必须保持该传递链。
