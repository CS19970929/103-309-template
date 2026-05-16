# 系统初始化与时基

## 相关文件

- [System_Init.c](../../Code/Source/System_Init.c)
- [System_Init.h](../../Code/Include/System_Init.h)
- [stm32f0xx_it.c](../../Code/Drivers/stm32f0xx_it.c)
- [system_stm32f0xx.c](../../Code/Drivers/system_stm32f0xx.c)

## 模块职责

系统初始化模块负责 MCU 时钟、GPIO 默认状态、阻塞延时、系统节拍、看门狗、唤醒 GPIO 和基础外设初始化。

## `InitDevice()` 顺序

| 顺序 | 函数 | 说明 |
| --- | --- | --- |
| 1 | `SystemInit()` | 配置系统时钟，默认 HSE + PLL。 |
| 2 | `InitSysTemMonitor()` | 初始化系统监控结构。 |
| 3 | `InitIO()` | 配置基础 GPIO 输出默认态。 |
| 4 | `InitDelay()` | 配置 SysTick 用于阻塞延时。 |
| 5 | `InitTimer()` | 配置 TIM17 系统节拍中断。 |
| 6 | `InitADC()` | 初始化 ADC、DMA、TIM15 触发。 |
| 7 | `Storage_Init()` | 初始化内部 Flash 参数区并加载/生成默认参数；旧 `InitE2PROM()` 只保留兼容包装。 |
| 8 | `Sci1_CommonUpper_Init()` | 条件启用 USART1 公共协议。 |
| 9 | `Sci2_CommonUpper_Init()` | 条件启用 USART2 公共协议。 |
| 10 | `InitAFE1()` | 初始化 BQ769x0 AFE。 |
| 11 | `InitWakeUp_NormalMode()` | 配置常规唤醒输入。 |
| 12 | `InitRTC()` | `PROJECT_FEATURE_RTC` 启用时初始化 RTC。 |
| 13 | `Init_IWDG()` | 初始化独立看门狗。 |

## 系统节拍

`TIM17` 是系统核心节拍源：

- 预分频基于 `SystemCoreClock / 1000000 - 1`，定时器计数单位为 1us。
- 自动重装值 `500 - 1`，每 500us 产生一次更新中断。
- 中断中累加两次形成 1ms，再派生其他任务标志。

主循环中大多数 `App_xxx()` 都依赖 TIM17 派生的全局节拍标志，因此 `TIM17` 停止会影响业务调度。当前 A002 模板没有 `IdleSleep` 源码，低功耗入口以 `SleepDeal` 为准。

## 阻塞延时

`InitDelay()` 使用 SysTick 作为阻塞延时计数基础，提供 `__delay_us()` 和 `__delay_ms()`。`SysTick_Handler()` 为空，说明 SysTick 不承担系统任务调度。

## 看门狗

`Init_IWDG()` 使用 LSI：

- Prescaler：64。
- 默认 reload：160。
- 当前低功耗路径没有独立 `IdleSleep` 看门狗重载逻辑；如后续新增 Stop/WFI 策略，必须同步设计 IWDG reload 和唤醒后恢复。

维护建议：新增长耗时流程必须确认是否会超过 IWDG 窗口；涉及内部 Flash 批量擦写或通信升级时应分段执行或显式喂狗。

## 基础 GPIO 默认状态

`InitIO()` 设置了以下基础输出：

| GPIO | 初始状态 | 说明 |
| --- | --- | --- |
| PB15 | High | `M_BLE_EN` / 电源控制相关。 |
| PA1 | Low | `MCUO_AFE_ALARM` / `M_AD_PWR`。 |
| PB1 | High | `M_CMNT_EN` / `PWSV_STB`。 |
| PB2 | Output | Debug LED。 |
| PC13 | Input | Key/DI/Wake 输入。 |
| PF7 | Output | AFE wake。 |

## 中断职责边界

- TIM17 中断只生成时基和标志，不直接执行业务控制。
- USART 中断只接收数据、检查故障并更新通信活动计数。
- EXTI 中断只设置唤醒/插入/负载状态标志。
- RTC 中断只记录唤醒原因并重设下一次 alarm。

这种边界有利于控制中断耗时，保持主循环承担业务状态机。
