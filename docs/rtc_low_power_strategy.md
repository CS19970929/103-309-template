# STM32F103 RTC 与低功耗策略调研

本文面向当前 `103 + 309` 工程，重点回答：

- `RTC_IT_SEC` 与 `RTC_IT_ALR` 的差异。
- `RTC_IT_SEC` 是否适合作为 STOP 唤醒源。
- 当前项目的 RTC/STOP 代码路径是否合理。
- 后续低功耗策略应如何分层，以及可以优化哪些点。

## 资料结论

### F103 低功耗模式边界

STM32F103xC/D/E 支持 Sleep、Stop、Standby 三类低功耗模式。官方资料的核心差异如下：

| 模式 | 保留 SRAM/寄存器 | 主时钟状态 | 典型唤醒源 | 适合当前项目的用途 |
| --- | --- | --- | --- | --- |
| Sleep | 保留 | CPU 停止，外设时钟继续 | 任意 NVIC 中断/事件 | 短空闲、等待 CAN/UART/按键等快速事件，不追求最低功耗 |
| Stop | 保留 | PLL/HSI/HSE 停止，RTC/LSE/LSI 可继续 | 配置到 EXTI 的中断/事件，包含 RTC Alarm 的 EXTI17 | 当前 RTC 周期唤醒、CAN 空闲探测、AFE/SOC 周期服务的主模式 |
| Standby | 不保留，仅 Backup/Standby 域保留 | 1.8V 域掉电 | WKUP、RTC Alarm、IWDG/NRST | 超长休眠或关机级省电；当前业务状态复杂，不建议作为主路径 |

官方 AN2629 明确说明，STOP 模式退出依赖 EXTI 线；用 RTC Alarm 从 STOP 唤醒时，需要配置 EXTI Line 17 为上升沿，并配置 RTC 产生 Alarm。Standby 下 RTC Alarm 可以作为唤醒源，但醒来后按复位流程重新启动。

### `RTC_IT_SEC` 与 `RTC_IT_ALR`

在 STM32F10x 标准外设库中：

- `RTC_IT_SEC` 是 RTC 秒中断，来自 RTC prescaler 产生的周期 tick，走 `RTC_IRQn` / `RTC_IRQHandler`。
- `RTC_IT_ALR` 是 RTC Alarm 中断，来自 `RTC_CNT == RTC_ALR` 比较，STOP 唤醒时通过内部 EXTI17 进入 `RTCAlarm_IRQn` / `RTCAlarm_IRQHandler`。

因此：

- `RTC_IT_SEC` 可以在运行态做每秒更新时间。
- `RTC_IT_SEC` 不适合作为 STOP 唤醒源。
- STOP 周期唤醒应使用 `RTC_IT_ALR + EXTI_Line17 + RTCAlarm_IRQn`。
- `RTC_GetITStatus(RTC_IT_ALR)` 同时检查 RTC 标志和中断使能位。如果唤醒后已经 `RTC_ITConfig(RTC_IT_ALR, DISABLE)`，它可能返回 `RESET`，此时排查唤醒源应看 `RTC_GetFlagStatus(RTC_FLAG_ALR)` 和 `EXTI_GetITStatus(EXTI_Line17)`。

## 当前项目代码路径

### 当前主流程

当前工程的 RTC 低功耗路径集中在以下文件：

- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/SleepDeal.c`

主要流程：

1. `rtc_sleep_prepare_rtc()` 调用：
   - `Can_PrepareSleep()`
   - `SOC_SaveSnapshotBeforeSleep()`
   - `Init_RTC()`
   - `IOstatus_RTCMode()`
   - `InitWakeUp_RTCMode()`
2. `InitWakeUp_RTCMode()` 调用 `RTC_WKTimeConfig()` 设置下一次 RTC Alarm。
3. `RTC_WKTimeConfig()`：
   - 关闭 `RTC_IT_ALR`
   - 清 `RTC_IT_ALR`、`RTC_FLAG_ALR`、`EXTI_Line17`
   - 设置 `RTC_SetAlarm(RTC_GetCounter() + wake_seconds)`
   - 重新打开 `RTC_IT_ALR`
4. `Sys_StopMode()` 执行 `PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI)`，醒来后恢复系统时钟。
5. `RTCAlarm_IRQHandler()` 调用 `RTC_HandleAlarmWakeup()`，设置：
   - `sys_time.rtc_alm_cnt++`
   - `is_rtc_wakekup = true`
6. `rtc_sleep_run_hiccup_cycle()` 根据 `is_rtc_wakekup` 做：
   - 统计休眠秒数
   - `Init()`
   - `isException()`
   - `update_rtc_soc()`
   - `Can_RtcWakeService()`

整体方向是正确的：当前项目已经把 STOP 主唤醒源放在了 `RTC_IT_ALR + EXTI17` 上，而不是依赖秒中断。

### 当前代码中容易误判的点

`RTC_IRQHandler()` 中同时处理秒中断和 Alarm：

```c
if (RTC_GetITStatus(RTC_IT_SEC) != RESET)
{
    RTC_ClearITPendingBit(RTC_IT_SEC);
    sys_time.rtc_sec_cnt++;
    TimeDisplay = 1;
    RTC_WaitForLastTaskSafe();
}

if (RTC_GetITStatus(RTC_IT_ALR) != RESET)
{
    RTC_HandleAlarmWakeup();
    sys_time.rtc_alm_cnt++;
}
```

但 STOP 模式下的 RTC Alarm 唤醒路径应主要看 `RTCAlarm_IRQHandler()`，而不是 `RTC_IRQHandler()`。调试时如果断在 `RTC_IRQHandler()` 的 `RTC_IT_ALR` 分支，可能会误判为“没有进入 Alarm 中断”。

当前源码已经把 Alarm 计数收敛到 `RTC_HandleAlarmWakeup()` 内部，`RTCAlarm_IRQHandler()` 和 `RTC_IRQHandler()` 只共用该处理函数，不再在外层重复维护 `sys_time.rtc_alm_cnt`。

## 推荐低功耗策略

### 当前项目建议主策略

当前项目是 BMS/通信/AFE/SOC 类业务，既需要低功耗，也需要保持 RAM 状态、保护状态、通信状态和统计计数。建议采用：

1. 主低功耗模式：STOP + RTC Alarm 周期唤醒。
2. 外部事件唤醒：GPIO EXTI 保留必要唤醒源，例如按键、充电器、RS485/UART 唤醒脚。
3. 运行态时间更新：仅在需要显示/日志时间时打开 `RTC_IT_SEC`，进入 STOP 前关闭秒中断。
4. 超长休眠/运输模式：后续再引入 Standby，但要设计 BootFlag、Backup Register、AFE 保持状态和复位后恢复流程。

### RTC 周期唤醒建议

进入 STOP 前建议固定执行以下顺序：

```c
PWR_BackupAccessCmd(ENABLE);

RTC_ITConfig(RTC_IT_SEC, DISABLE);
RTC_ClearITPendingBit(RTC_IT_SEC);

RTC_ITConfig(RTC_IT_ALR, DISABLE);
RTC_ClearITPendingBit(RTC_IT_ALR);
RTC_ClearFlag(RTC_FLAG_ALR);
RTC_WaitForLastTaskSafe();
EXTI_ClearITPendingBit(EXTI_Line17);

wake_seconds = RTC_GetWakeupPeriodSeconds();
RTC_SetAlarm(RTC_GetCounter() + wake_seconds);
RTC_WaitForLastTaskSafe();

RTC_ITConfig(RTC_IT_ALR, ENABLE);
RTC_WaitForLastTaskSafe();
```

醒来后：

```c
cpu_frequency_conf();
RTC_WaitForSynchroSafe();

if (is_rtc_wakekup)
{
    /* RTC 周期服务 */
}
else
{
    /* GPIO/外部事件唤醒源判断 */
}
```

### 不同业务场景策略

| 场景 | 建议模式 | 唤醒源 | 说明 |
| --- | --- | --- | --- |
| 主循环短暂空闲 | Sleep | SysTick、CAN、UART、GPIO | 不建议为省电大改业务；可作为后续轻量优化 |
| BMS 空闲但需周期检测 | Stop | RTC Alarm + 必要 GPIO EXTI | 当前最适合项目的主模式 |
| CAN 空闲探测 | Stop | RTC Alarm，唤醒后短时间 InitCan + probe | 与当前 `Can_RtcWakeService()` 思路一致 |
| 按键/充电器唤醒 | Stop | GPIO EXTI | 需要保证 STOP 前没有误挂起 pending bit |
| 长时间运输/仓储 | Standby | WKUP 或 RTC Alarm | 需接受复位启动和 RAM 丢失；当前不建议直接替换 STOP |
| 看门狗开启 | Stop/Standby 均需重新评估 | IWDG 不可停止 | 当前 `RTC_GetWakeupPeriodSeconds()` 已按 IWDG 窗口限制，是正确方向 |

## 当前项目可优化点

### 优先级 P1：进入 STOP 前关闭秒中断

当前 `RTC_ClockConfig()` 和 LSI fallback 中默认 `RTC_ITConfig(RTC_IT_SEC, ENABLE)`。如果进入 STOP 前不关闭 `RTC_IT_SEC`，秒中断会导致运行态频繁进 `RTC_IRQHandler()`，并可能干扰调试判断。

建议：

- 运行态需要更新时间时打开 `RTC_IT_SEC`。
- 进入 STOP 前在 `RTC_WKTimeConfig()` 或更上层 sleep prepare 中关闭 `RTC_IT_SEC`。
- STOP 唤醒后如需要 `App_RTC()` 秒级显示，再重新打开。

### 优先级 P1：修正 Alarm 计数重复

`RTC_HandleAlarmWakeup()` 已经 `sys_time.rtc_alm_cnt++`，`RTC_IRQHandler()` 的 Alarm 分支不应再次加 1。

建议删除 `RTC_IRQHandler()` 中 Alarm 分支的第二次计数，或者把计数统一放到外层，但不要双处维护。

### 优先级 P1：不要用 `RTC_GetITStatus(RTC_IT_ALR)` 单独判断唤醒源

用于中断服务中可以；用于 STOP 醒来后的诊断不够稳。因为它依赖中断使能位。

建议在调试日志中同时打印：

```c
RTC_GetFlagStatus(RTC_FLAG_ALR)
EXTI_GetITStatus(EXTI_Line17)
is_rtc_wakekup
sys_time.rtc_alm_cnt
sys_time.rtc_irq_cnt
sys_time.rtc_sec_cnt
```

当前 `rtc_sleep_dump_state()` 已经打印 `RTC_FLAG_ALR` 和 `EXTI_Line17`，方向正确。可以把 `rtc_alm_cnt/sec_cnt/irq_cnt` 也加进去。

### 优先级 P2：`Init()` 中 RTC 唤醒分支命名和逻辑可读性

当前：

```c
void Init(void)
{
    if (is_rtc_wakekup)
    {
        InitRtcWakeupCheck();
    }
    else
    {
        InitRunAfterStopWakeup();
    }
}
```

从命名看，`InitRunAfterStopWakeup()` 更像“STOP 醒来后的恢复”，但当前在非 RTC 唤醒分支调用；`InitRtcWakeupCheck()` 只初始化 Delay/AFIO/SCI/IIC/EEPROM，更像“RTC 短服务初始化”。建议后续重命名为：

- `InitForRtcShortWakeService()`
- `InitForExternalWakeFullRun()`

这个优化不改变行为，但能减少后续维护误判。

### 优先级 P2：STOP 前统一清所有相关 EXTI pending

AN2629 强调，进入 STOP 前必须清 EXTI pending 和 RTC Alarm flag，否则 STOP 进入可能被忽略，程序看起来像“刚进就醒”。

当前 `RTC_ClearAlarmPending()` 已清 EXTI17；但项目还有 EXTI0/3/5/6/7/12/13 等唤醒源。建议做一个统一函数：

```c
static void LowPower_ClearWakePending(void)
{
    EXTI_ClearITPendingBit(EXTI_Line0);
    EXTI_ClearITPendingBit(EXTI_Line3);
    EXTI_ClearITPendingBit(EXTI_Line5);
    EXTI_ClearITPendingBit(EXTI_Line6);
    EXTI_ClearITPendingBit(EXTI_Line7);
    EXTI_ClearITPendingBit(EXTI_Line12);
    EXTI_ClearITPendingBit(EXTI_Line13);
    EXTI_ClearITPendingBit(EXTI_Line17);
}
```

实际清哪些线要以当前宏开关和硬件电平为准。

### 优先级 P2：LSE/LSI 策略需要明确

当前 `RTC_ClockConfig()` 优先 LSE，失败后 fallback 到 LSI。这是合理的容错策略。

但 LSI 频率误差较大，适合 watchdog/粗略唤醒，不适合长期 SOC 休眠时间累计。当前 `s_u32RtcSleepElapsedSeconds` 会影响 SOC 休眠补偿，因此建议：

- 生产硬件应优先保证 LSE 正常。
- 如果 fallback 到 LSI，记录状态位或日志，并适当降低 SOC 休眠补偿置信度。
- RTC SOC 校准必须继续按稳定窗口和 `10min/step` 节拍执行，不能因为 RTC 唤醒周期是 `1s/10s` 就每次唤醒修正；普通 RTC OCV 目标与当前 SOC 误差 `<=3%` 时不校准，且永远不向上校准。
- `DEEP_MODE` 现在也配置 RTC Alarm 周期唤醒。深睡循环只把 RTC 唤醒周期累计到 `BKP_DR13~BKP_DR17`，不在 SOC/AFE 未完整初始化时直接校准；真正按键或充电唤醒并完成正常初始化后，首个有效 AFE 样本达到 `PROJECT_CFG_SOC_RTC_CALIBRATION_MIN_SECONDS` 才允许一次 OCV 小步校准。深睡 RTC OCV 不套普通 `3%` deadband，但仍只能按 OCV 向下校准 `1%`，不能跳变或向上校准。
- CAN RTC 唤醒服务仍由 `Can_RtcWakeService()` 控制上电窗口；每个发送批次结束后会关闭收发器并请求 bxCAN sleep，下一次 RTC probe 或周期广播前再唤醒外设。
- 后续可以在 Backup Register 记录 RTC 时钟源，便于售后诊断。

### 优先级 P3：Standby 暂不作为主路径

Standby 功耗更低，但代价是 RAM/寄存器丢失，醒来后复位启动。当前项目已有：

- CAN 逻辑 tick
- AFE/IIC 状态
- SOC 休眠补偿
- BootFlag
- LED/ADC/SCI/CAN 多模块恢复

直接替换成 Standby 风险较大。建议只有在“运输模式/长期仓储模式”中单独设计：

- 写 BootFlag 和关键 Backup Register。
- 配置 RTC Alarm 或 WKUP。
- 进入 Standby。
- Reset 后按 BootFlag 走最小恢复路径。

## 建议落地顺序

1. 先做小改：STOP 前关闭 `RTC_IT_SEC`，修正 `rtc_alm_cnt` 重复计数。
2. 增强日志：`rtc_sleep_dump_state()` 增加 RTC 三个计数和 `RTC_GetCounter()`。
3. 抽出 `LowPower_ClearWakePending()`，统一清 RTC/GPIO 唤醒 pending。
4. 重命名短服务/完整恢复初始化函数，降低维护误判。
5. 评估是否需要独立 Standby 运输模式，而不是替换当前 STOP 周期唤醒。

## 参考资料

- ST AN2629, STM32F101xx/102xx/103xx low-power modes: https://www.st.com/resource/en/application_note/an2629-stm32f101xx-stm32f102xx-and-stm32f103xx-lowpower-modes-stmicroelectronics.pdf
- ST STM32F103xC/D/E datasheet: https://www.st.com/resource/en/datasheet/stm32f103zd.pdf
- ST Wiki, Getting started with PWR: https://wiki.stmicroelectronics.cn/stm32mcu/wiki/Getting_started_with_PWR
- ST Wiki, Getting started with RTC: https://wiki.stmicroelectronics.cn/stm32mcu/wiki/Getting_started_with_RTC
- ST Community, How to configure the RTC to wake up the STM32 from Low Power modes: https://community.st.com/t5/stm32-mcus/how-to-configure-the-rtc-to-wake-up-the-stm32-from-low-power/ta-p/49836
- ST Community, STOP mode in STM32F103: https://community.st.com/t5/stm32-mcus-products/stop-mode-in-stm32f103/td-p/624805
