# STOP 唤醒后时钟恢复设计

> ClockAgent 第一阶段/第二阶段交界设计草案。本文只给方案，不修改源码。

## 1. 设计目标

- 面向当前 STM32F103C8 BMS 项目，先保证 STOP 进入/退出稳定、通信时钟不乱、IWDG 不误复位，再考虑更低电流。
- 保持现有协议不变：CAN 仍按当前源码目标 250 kbit/s，USART/Modbus/RS485 仍按当前 `USART_BaudRate = 19200`。
- Stop 返回后先恢复系统时钟，再恢复 SysTick、TIM、ADC、USART、CAN、LED、AFE。
- 不重置 RTC Backup Domain，不破坏 RTC Alarm/EXTI17 唤醒配置。

## 2. 官方规则约束

- ST RM0008 对 STM32F10x STOP 模式的规则是：进入 STOP 后 1.8 V 域时钟停止，PLL、HSI、HSE 振荡器关闭；退出 STOP 时 HSI RC 先成为系统时钟。RCC `SW[1:0]` 位说明也明确离开 Stop/Standby 时硬件会强制选择 HSI。官方资料入口：ST RM0008《STM32F101xx, STM32F102xx, STM32F103xx, STM32F105xx and STM32F107xx reference manual》，https://www.st.com/resource/en/reference_manual/rm0008-stm32f103xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf 。
- 因此只要运行态使用 HSE 或 PLL，STOP 返回后都必须重新配置系统时钟。当前项目运行态使用 HSE 直驱 SYSCLK，所以必须恢复 HSE、SYSCLK、HCLK、PCLK1、PCLK2。

## 3. 当前项目恢复链路基线

当前链路已经满足“先恢复时钟、再恢复外设”的基本顺序：

1. `103 + 309/Project/Source/rtc_sleep.c` 第 303-321 行：`rtc_sleep_run_hiccup_cycle()` 调用 `RtcSleep_PortEnterStop()` 进入 STOP，返回后调用 `RtcSleep_PortRestoreAfterStop()`。
2. `103 + 309/Project/Source/rtc_sleep_port.c` 第 118-123 行：`RtcSleep_PortEnterStop()` 在 `Sys_StopMode()` 前后喂 IWDG。
3. `103 + 309/Project/Source/conf/conf.c` 第 374-385 行：`Sys_StopMode()` 在 `PWR_EnterSTOPMode()` 返回后立即调用 `cpu_frequency_conf()`。
4. `103 + 309/Project/Source/rtc_sleep_port.c` 第 207-212 行：`cpu_frequency_conf()` 调用 `SystemInit()`、`SystemCoreClockUpdate()`、`InitDelay()`。
5. `103 + 309/Project/Source/conf/conf.c` 第 392-421 行：`InitRunAfterStopWakeup()` 再恢复 RTC 中断、GPIO、ADC、USART、CAN、TIM3、AFE I2C。

该链路第一版可以继续作为行为基线，但建议把 `cpu_frequency_conf()` 的内部实现替换为专用 Clock 模块接口。

## 4. 最小可行 Clock 模块接口

后续最小实现建议新增 `bsp_clock.c/.h`，接口先保持窄：

```c
void BspClock_InitRunClock(void);
uint8_t BspClock_RestoreAfterStop(void);
uint32_t BspClock_GetSystemClockHz(void);
uint32_t BspClock_GetLastRestoreFlags(void);
```

建议 `cpu_frequency_conf()` 先改成兼容壳：

```c
void cpu_frequency_conf(void)
{
    (void)BspClock_RestoreAfterStop();
    SystemCoreClockUpdate();
    InitDelay();
}
```

这样现有 `Sys_StopMode()`、`rtc_sleep_port.c`、`SleepDeal.c` 调用点不需要大规模调整，后续再逐步把命名收敛到 `LP_AfterWakeup()` 或 `Clock_ReConfigAfterStop()`。

## 5. 当前项目的目标时钟配置

源码目标配置必须以 `system_stm32f10x.c` 为准：

- `SYSCLK_FREQ_HSE` 已启用，`SYSCLK_FREQ_72MHz` 被注释，见 `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/system_stm32f10x.c` 第 106-116 行。
- `HSE_VALUE` 默认是 8 MHz，见 `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x.h` 第 115-120 行。
- `SetSysClockToHSE()` 配置 `HCLK = SYSCLK`、`PCLK2 = HCLK`、`PCLK1 = HCLK`、`SYSCLK = HSE`，见 `system_stm32f10x.c` 第 552-568 行。

因此当前版本 `BspClock_RestoreAfterStop()` 的目标状态应为：

- HSE ON 且 ready。
- SYSCLK source = HSE。
- AHB prescaler = DIV1。
- APB1 prescaler = DIV1。
- APB2 prescaler = DIV1。
- PLL OFF 或不参与系统时钟。
- `SystemCoreClock` 更新到 `HSE_VALUE`。

## 6. 推荐恢复顺序

STOP 返回后的推荐顺序：

1. 从 `PWR_EnterSTOPMode()` 返回。
2. 不访问依赖准确 APB/AHB 时钟的外设业务。
3. 调用 `BspClock_RestoreAfterStop()`：
   - 确保 HSI 可用，作为恢复过程的临时安全时钟。
   - 启动 HSE，带超时等待 `HSERDY`。
   - 配置 Flash latency/prefetch 与 AHB/APB 分频。
   - 切换 SYSCLK 到 HSE，带超时等待 `SWS=HSE`。
   - 调用 `SystemCoreClockUpdate()`。
   - 返回成功/失败状态，不要静默失败。
4. 调用 `InitDelay()`。
5. 调用 `RTC_RestoreRunInterrupts()`，再恢复 GPIO、ADC、USART、CAN、TIM、LED、AFE。

当前源码对应点是 `conf.c` 第 382-385 行。后续实现时应保持时钟恢复仍位于 `PWR_EnterSTOPMode()` 后、`InitRunAfterStopWakeup()` 前。

## 7. 错误处理策略

当前 `SetSysClockToHSE()` 在 HSE 启动失败时分支为空，见 `system_stm32f10x.c` 第 570-573 行。后续专用恢复函数不应继续静默失败，建议：

- 返回 `BSP_CLOCK_RESTORE_HSE_TIMEOUT` 或设置 `BspClock_GetLastRestoreFlags()`。
- 如果 HSE 失败，只允许短暂降级到 HSI 运行保护逻辑、记录故障、喂狗、尝试再次恢复。
- HSE 失败时不要立即恢复 CAN 正常通信。当前 CAN 位时序在 `Can_HDX.c` 第 839-843 行按 PCLK1 计算；HSI 精度不足时，即使名义频率也是 8 MHz，也不适合作为长期 CAN 时钟依据。
- USART/Modbus 可以作为降级诊断通道，但必须明确标记时钟降级状态，避免把不稳定通信当正常业务通信。

## 8. PLL/72 MHz 扩展规则

当前项目不使用 PLL；如果后续切换到 72 MHz，必须同步完成：

- 改 `system_stm32f10x.c` 的目标宏，不能只改注释。
- APB1 必须分频到不超过 36 MHz；否则 TIM2/TIM3/TIM4、USART2/3、CAN 的时钟假设全部失效。
- 重新计算 CAN `CAN_Prescaler`、`CAN_BS1`、`CAN_BS2`，当前 `8 MHz / 4 / 8 = 250 kbit/s` 的关系不再成立，见 `Can_HDX.c` 第 839-843 行。
- 重新确认 `RCC_ADCCLKConfig(RCC_PCLK2_Div8)` 后 ADC 时钟是否满足 STM32F103 ADC 最大频率要求，当前调用点在 `ADC.c` 第 232 行。
- 重新确认 `Timer_GetPrescalerFor100kHz()`、`InitADC_TIMER()`、`LedBar_GetTimerPrescalerFor100kHz()` 是否都基于更新后的 `SystemCoreClock`。

## 9. 和低功耗框架的集成位置

未来 `app_lowpower` 的建议调用关系：

```text
LP_EnterStop(seconds)
  -> LP_BeforeSleep()
     -> 关闭/挂起 TIM3、TIM4、ADC、CAN、USART TX、LED 扫描
     -> 配置 RTC Alarm / EXTI pending
  -> PWR_EnterSTOPMode(...)
  -> BspClock_RestoreAfterStop()
  -> LP_AfterWakeup()
     -> InitDelay()
     -> RTC_RestoreRunInterrupts()
     -> GPIO/ADC/USART/CAN/TIM/LED/AFE 恢复
```

在当前代码中，短期可保持 `Sys_StopMode()` 作为唯一 STOP 入口；不要让多个模块各自调用 `PWR_EnterSTOPMode()`，否则时钟恢复顺序会变得不可审计。

## 10. 当前不建议修改的内容

- 第一阶段不修改源码。
- 当前不建议改 CAN/USART 协议参数。
- 当前不建议从 HSE 直驱切到 72 MHz PLL，除非先完成 CAN、ADC、TIM、USART 的完整时钟参数复核。
- 当前不建议把 RTC Backup Domain 和系统时钟恢复放在同一个函数里；RTC LSE/LSI 选择应继续由 RTC 模块维护。
- 当前不建议为了最低电流关闭更多调试或外设路径；先保证 STOP 唤醒后时钟、通信、采样、IWDG 顺序稳定。

## 11. 最小验证清单

- 进入 STOP 前记录 `RCC->CFGR`、`RCC->CR`、`SystemCoreClock`。
- STOP 唤醒后、恢复外设前记录 `RCC->CFGR`、`RCC->CR`，确认曾回到 HSI。
- `BspClock_RestoreAfterStop()` 后确认 `SWS=HSE`、`SystemCoreClock=HSE_VALUE`、`PCLK1/PCLK2` 符合设计。
- 恢复 `InitTimer()` 后确认 TIM3 10 ms tick 正常推进。
- 恢复 `InitCan()` 后确认 250 kbit/s CAN 帧可收发。
- 恢复 `AppInit_InitSci()` 后确认 19200 Modbus/RS485 收发正常。
- HSE 启动失败注入测试：确认不会静默进入“看似正常但 CAN 时钟不可靠”的状态。
