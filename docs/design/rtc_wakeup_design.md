# RTC 周期唤醒设计建议

本文档是 RtcAgent 第一阶段设计输出，仅提出方案，不修改源码。当前项目 MCU 已判断为 STM32F103C8，因此第一版应继续使用 STM32F1 的 RTC Alarm + EXTI17 实现 Stop 周期唤醒；F0 Wakeup Timer 只作为后续跨项目移植分支设计。

## 设计边界

- 第一阶段不改源码，只记录分析和设计。
- 当前项目不切换协议、不改变 CAN/Modbus/RS485 报文。
- 当前项目第一版只做 Stop + RTC 周期唤醒，不做 CAN/USART Stop 唤醒。
- 不用 F0 Wakeup Timer 改写当前 F1 工程。
- 不追求最低电流优先，先保证稳定睡眠、稳定唤醒、通信不乱、保护不丢、IWDG 不误复位。

## 官方规则摘要

- STM32F10x 支持 Sleep、Stop、Standby 三类低功耗模式；Stop 模式 SRAM/寄存器保持，Standby 只保留 Standby 电路和 Backup domain。
- F10x Stop 模式中 HSI/HSE/PLL 会关闭，RTC、LSI、LSE 可按相应控制位保持；Stop 唤醒后系统时钟选择 HSI，因此必须恢复系统时钟。
- F10x RTC 自动唤醒 Stop/Standby 可由 LSE 或 LSI 驱动。Stop 模式 RTC Alarm 唤醒必须配置 EXTI Line17 上升沿，并配置 RTC 产生 alarm。
- F10x RTC APB 接口在复位、APB 停止、Stop/Standby 唤醒后读取寄存器前要等待 `RSF`；写 RTC 寄存器前要等待 `RTOFF`。
- F0 移植时要按 RM0091/RM0360 区分：RTC Alarm 通常走 EXTI17，RTC Wakeup Timer 走 EXTI20；部分 F030 型号没有完整 Wakeup Timer。

## 当前项目可复用部分

- MCU/库绑定：`CommomSH367309_16series_103RCT6_C.uvprojx:17-19` 和 `:340-342` 已固定为 STM32F103C8、`STM32F10X_MD`、标准外设库。
- RTC 使能：`Project_Config.h:84-86` 打开 `PROJECT_CFG_RTC_ENABLE`，`conf.h:53-55` 定义 `__FUNC_RTC__`。
- 初始化入口：`AppInit.c:66-71` 启动时调用 `Init_RTC()`。
- RTC 时钟：`RTC.c:206-279` 已有 LSE 优先、LSI 兜底；`RTC.c:26-55` 已有带超时的同步等待函数。
- Alarm 设置：`RTC.c:408-418` 已封装 `RTC_WKTimeConfig()`，`RTC.c:305-317` 已封装 `RTC_EnableAlarmAfterSeconds()`。
- EXTI17/NVIC：`RTC.c:326-351` 已配置 `EXTI_Line17` 和 `RTCAlarm_IRQn`。
- IRQ 标志：`RTC.c:494-520` 已将 Alarm 唤醒转为 `is_rtc_wakekup=true`。
- Stop 入口：`conf.c:374-385` 已集中封装 `Sys_StopMode()`。
- Stop 恢复：`conf.c:392-421` 已集中恢复 Delay、RTC、IO、ADC、USART、CAN、TIM、AFE。
- 业务流程：`rtc_sleep.c:303-345` 已实现 RTC 唤醒后累计休眠秒数、SOC 休眠补偿和 CAN RTC 服务窗口。

## 推荐的 F1 RTC Stop 唤醒流程

### 初始化阶段

1. 使能 PWR/BKP 时钟并打开 Backup domain 写访问。当前可复用 `RTC.c:20-24`。
2. 读取 `BKP_DR1` 判断 RTC 是否已初始化。当前可复用 `RTC.c:442-461`。
3. 完整初始化时优先启动 LSE，并设置 `RTC_SetPrescaler(32767)`；LSE 超时后回退 LSI，并设置 `RTC_SetPrescaler(40000 - 1)`。当前可复用 `RTC.c:235-275` 和 `RTC.c:96-124`。
4. 所有 `RSF/RTOFF` 等待必须带超时。当前已有 `RTC_WaitForSynchroSafe()` 和 `RTC_WaitForLastTaskSafe()`，后续要保证每个调用点都检查返回值。
5. EXTI17/NVIC 可以初始化一次，但 Alarm 只在进入 Stop 前临时打开。

### 进入 Stop 前

1. 业务层确认可睡眠：无通信活动、无 Flash 写入、无 AFE 忙、无保护处理、IWDG 周期安全。
2. 保存核心状态：当前 `LowPowerSleep_SaveCoreState()` 已保存 CAN、SOC、老化进度，见 `LowPowerSleep.c:5-10`。
3. 关闭 RTC 秒中断，避免秒中断扰动 Stop 唤醒源判断。当前 `RTC_WKTimeConfig()` 已调用 `RTC_DisableSecondInterrupt()`。
4. 清 `RTC_IT_ALR/RTC_FLAG_ALR/EXTI_Line17/RTCAlarm_IRQn` pending。当前 `RTC_ClearAlarmPending()` 已覆盖这些对象。
5. 设置 `RTC_SetAlarm(RTC_GetCounter() + wake_seconds)` 并使能 `RTC_IT_ALR`。
6. `wake_seconds` 必须小于 IWDG 安全窗口；当前 `RTC_GetWakeupPeriodSeconds()` 里的 IWDG 限制在 `RTC.c:375-397` 被注释，后续实现时应恢复为强制裁剪。
7. 进入 Stop 前清普通唤醒 EXTI pending。当前 `Sys_StopMode()` 调用 `LowPower_ClearWakeupPending()`，但该函数不清 EXTI17；EXTI17 由 RTC 模块清，后续应保留模块归属或明确集中清除策略。

### Stop 中

1. 使用 `PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI)`。当前在 `conf.c:381-382`。
2. 不依赖 TIM3/SysTick 运行；Stop 中主时钟停止，RTC 由 LSE/LSI 保持。
3. IWDG 若已开启仍会继续运行，RTC 唤醒周期必须保证喂狗窗口。

### 唤醒后

1. 首先恢复系统时钟和 `SystemCoreClock`，然后恢复 delay 基准。当前 `cpu_frequency_conf()` 在 `rtc_sleep_port.c:207-212` 调用 `SystemInit()`、`SystemCoreClockUpdate()`、`InitDelay()`。
2. 禁用 Stop 专用 Alarm，恢复运行态 RTC 秒中断。当前 `RTC_RestoreRunInterrupts()` 在 `RTC.c:426-435`。
3. 恢复 IO、ADC、USART、CAN、TIM3、AFE。当前 `InitRunAfterStopWakeup()` 在 `conf.c:392-421` 执行。
4. 如果 `is_rtc_wakekup` 为真，按计划周期累计休眠秒数；如果是外部唤醒，应退出 RTC 周期睡眠，不能把计划周期当成真实休眠时长。
5. 唤醒后重新同步 AFE/MOS/保护状态，当前 `rtc_sleep.c:241-263` 已在 RTC 醒来后调用 AFE 数据更新和异常判断。

## 后续 F0 移植分支

F0 项目建议把 RTC 底层拆成条件编译分支，不要把 F1 的 `RTC_SetAlarm(RTC_GetCounter()+seconds)` 直接搬过去。

- F0 Alarm 分支：用于日历 Alarm A/B，Stop 唤醒走 EXTI17。
- F0 Wakeup Timer 分支：用于周期秒级唤醒，Stop 唤醒走 EXTI20，必须处理 `WUTE` 关闭等待、`WUTF` 清除、`WUTR` 设置、`WUTIE` 使能。
- F0 初始化分支：处理 RTC 写保护、初始化模式、异步/同步预分频、`RSF` 同步、Alarm/WUT pending 清除。
- F0 型号兼容：按具体芯片参考手册确认是否支持 Wakeup Timer；若不支持，退回 Alarm 周期唤醒。

## 建议抽象接口

后续最小实现可把当前 `RTC.c` 中可复用逻辑迁移或包裹到 `bsp_rtc`，不要一次重构业务层。

```c
uint8_t BspRtc_Init(void);
uint8_t BspRtc_SetStopWakeAfterSeconds(uint32_t seconds);
void BspRtc_DisableStopWake(void);
void BspRtc_RestoreRunInterrupts(void);
uint8_t BspRtc_IsAlarmWake(void);
void BspRtc_ClearAlarmWake(void);
uint32_t BspRtc_GetLastWakeSeconds(void);
uint8_t BspRtc_IsClockHealthy(void);
```

与建议低功耗框架接口的映射：

- `LP_SetWakeupPeriod(seconds)` 调用 `BspRtc_SetStopWakeAfterSeconds(seconds)` 前只保存目标周期，不直接进 Stop。
- `LP_EnterStop(seconds)` 先检查 `LP_CanSleep()`，再设置 RTC Alarm，最后进入 `Sys_StopMode()`。
- `LP_AfterWakeup()` 先恢复时钟，再调用 `BspRtc_DisableStopWake()` 和 `BspRtc_RestoreRunInterrupts()`。
- `LP_GetLastSleepSeconds()` 对 RTC Alarm 唤醒返回计划周期，对外部唤醒返回 0 或由进入/退出 counter 差值估算，不能盲目返回计划周期。

## 当前代码建议修正点

这些是后续阶段的建议，当前阶段不改源码。

1. `RTC_WaitForLastTaskSafe()` 返回值要全链路检查。重点是 `RTC_ClearAlarmPending()`、`RTC_DisableSecondInterrupt()`、`RTC_DisableAlarmInterrupt()`、`RTC_EnableAlarmAfterSeconds()`、`RTC_RestoreRunInterrupts()`。
2. 恢复 `RTC_GetWakeupPeriodSeconds()` 中 IWDG 安全窗口裁剪，或移到未来 `LP_CanSleep()` 中形成 `LP_BLOCK_IWDG_UNSAFE`。
3. 建立 BKP 寄存器分配表，避免 `BKP_DeInit()` 清除睡眠标志、老化进度、故障原因。
4. 将 EXTI17 pending 清除归属明确化：保持 RTC 模块负责，或在 `LowPower_ClearWakeupPending()` 增加 RTC 参数化清除；不要双边无序清。
5. 检查 `App_RTC()` 是否需要纳入运行调度，否则 `RTC_time` 对上位机读寄存器可能不是实时值。
6. 把 `is_rtc_wakekup` 更名或封装为 `rtc_alarm_wakeup`，当前拼写 `wakekup` 不影响功能，但不利于后续公共框架。

## 不建议现在修改

- 不建议把当前 F1 工程改成 F0 Wakeup Timer 模式。
- 不建议第一版做 CAN/USART Stop 唤醒；通信活跃时应禁止休眠。
- 不建议把 `Sys_StopMode()` 和所有外设恢复一次性大重构；当前先包一层低功耗框架更稳。
- 不建议在 RTC IRQ 里做 AFE、SOC、CAN 服务；IRQ 只清标志和置位状态。
- 不建议删除现有 `SleepDeal.c` 的 reset-style 休眠路径；应先兼容，再逐步收口。

## 验证重点

- LSE 正常：RTC 走 LSE，Stop 后 Alarm 能周期唤醒。
- LSE 异常：RTC 自动回退 LSI，不死等，不清除关键业务 BKP 状态。
- 调试器连接：不会卡死在 `RSF/RTOFF` 等待。
- IWDG 开启：RTC 周期小于 IWDG 安全窗口，Stop 前后喂狗正常。
- 外部唤醒：PA0 充电、按键、AFE/MCU_WK 能退出 RTC 周期睡眠，不误累计整周期。
- 通信活跃：`RTC_ExtComCnt` 变化时不进入 RTC Stop。
- 唤醒恢复：SystemClock、Delay、TIM3、ADC、USART、CAN、AFE 恢复后主循环继续运行。
