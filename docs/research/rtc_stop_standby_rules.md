# RTC + Stop/Standby 低功耗规则清单

更新时间：2026-05-26  
阶段：第一阶段，只读资料调研  
目标：把官方资料转成后续设计和代码审查可直接执行的规则。

## 规则等级

- P0：不满足可能导致无法唤醒、误复位、协议异常、保护状态丢失。
- P1：不满足会导致功耗不稳定、时间不准、偶发唤醒异常或调试/量产差异。
- P2：优化项或后续增强项。

## P0 必须遵守

### P0-1 首版只做 Stop + RTC 周期唤醒

依据：

- RM0008/RM0091/RM0360：Stop 保持 SRAM 和多数寄存器，Standby 唤醒是 reset 流程。
- PM0056/PM0215：Deep-sleep 在产品级对应 Stop 或 Standby。

规则：

- 普通空闲低功耗使用 Stop，不使用 Standby。
- Standby 只用于明确的复位式深度休眠，例如过放深度休眠。
- Stop 前后不能改变现有 Modbus/CAN 协议状态机语义；通信活跃时直接禁止休眠。

### P0-2 RTC 唤醒时钟源必须可在 Stop/Standby 中工作

依据：

- RM0008/RM0091/RM0360：Stop 中 HSI/HSE/PLL 停止；RTC 可由 LSE/LSI/HSE 分频等来源提供，但低功耗唤醒不能依赖会停止的 HSE。
- AN2867：LSE 设计和起振可靠性需要硬件保证。

规则：

- 优先使用 LSE。
- LSE 启动失败时允许降级到 LSI，但必须记录/暴露降级状态。
- 禁止把 HSE 分频作为 Stop/Standby 周期唤醒的唯一来源。
- 等待 LSE ready、LSI ready 必须有超时。

### P0-3 Stop 唤醒后必须恢复系统时钟

依据：

- RM0091/RM0360：从 Stop/Deep-sleep 唤醒后 HSI 被选为系统时钟。
- RM0008 也给出 Stop 唤醒后需要重新配置系统时钟的约束。

规则：

- `LP_AfterWakeup()` 第一类动作是恢复 HSE/PLL/SYSCLK/AHB/APB。
- 恢复系统时钟后才能恢复依赖时钟的 SysTick、TIM、ADC、UART、CAN、LED 刷新。
- 不允许唤醒后直接按休眠前的 CAN/UART 波特率配置继续收发。

### P0-4 IWDG 超时时间必须覆盖 RTC 休眠周期

依据：

- RM0008 IWDG：IWDG 使用 LSI，VDD 域在 Stop/Standby 仍工作；40 kHz 标称下 prescaler 256、reload 4095 最大约 26.2 s，但 LSI 可偏差。
- F1 数据手册：LSI 典型 40 kHz，范围可到 30-60 kHz。
- RM0091/RM0360：IWDG clock always LSI。

规则：

- 睡前立即喂 IWDG。
- `RTC_WAKEUP_PERIOD_SEC < IWDG_MIN_TIMEOUT_SEC`。
- 若未测量 LSI，F1 按 60 kHz 估算最短 IWDG 超时；例如 prescaler 256、reload 4095 时最短约 17.5 s。
- 默认建议 RTC 周期小于 IWDG 最短超时的 50%-70%。
- 若需要长于 IWDG 的低功耗时间，必须走 Standby/复位式策略，不允许在 Stop 中赌 IWDG 不复位。

### P0-5 进入 Stop 前必须清 pending 并确认无禁止休眠原因

依据：

- RM0091/RM0360：进入 Stop 前应清 EXTI pending、外设中断 pending、RTC Alarm flag，否则可能立即退出或无法进入。
- PM0215：WFI/WFE 会被中断/事件唤醒，也可能被伪唤醒打断。

规则：

- 清 RTC Alarm/Wakeup flag。
- 清对应 EXTI_PR line。
- 清 NVIC pending 或处理完外设 pending。
- Flash 忙、通信活跃、AFE 忙、升级中、故障处理、LED 显示窗口、按键处理都必须返回禁止休眠原因。
- 进入 Stop 后若马上醒来，必须记录 wake reason 和 block reason，避免无诊断地反复进出。

### P0-6 F1 RTC Stop 唤醒固定走 Alarm + EXTI17

依据：

- RM0008：STM32F1 RTC 是 counter + alarm；RTC Alarm 从 Stop 唤醒需要 EXTI Line 17 上升沿；从 Standby 唤醒不需要 EXTI17。

建议顺序：

1. 开启 PWR/BKP 时钟。
2. 允许访问 Backup domain。
3. 选择 LSE 或 LSI 作为 RTCCLK，并使能 RTC。
4. 等待 RTC 同步，等待上一次写操作完成，所有等待必须有超时。
5. 读取当前 RTC counter，设置 Alarm = counter + seconds。
6. 清 RTC Alarm flag。
7. 配置 EXTI Line 17 上升沿 interrupt/event。
8. 清 EXTI Line 17 pending。
9. 使能 RTC Alarm 中断/事件。
10. 喂 IWDG，进入 Stop。
11. 醒后先恢复系统时钟，再清 RTC/EXTI 标志并更新 `last_sleep_seconds`。

### P0-7 F0 RTC 必须区分 Wakeup Timer 和 Alarm

依据：

- RM0091/RM0360：RTC Alarm 通过 EXTI17；RTC Wakeup event 在部分 F0 通过 EXTI20。
- RM0360：STM32F070xB、STM32F030xC 支持 RTC periodic wakeup；STM32F030x4/x6/x8 不支持该 periodic wakeup 功能。
- AN4759：Wakeup Timer 配置需要关 WUTE、等待 WUTWF、配置 WUTR/WUCKSEL、清 flag、再使能。

规则：

- 框架必须提供编译期能力宏，例如 `BSP_RTC_HAS_WAKEUP_TIMER`。
- 支持 Wakeup Timer 的 F0：优先使用 Wakeup Timer + EXTI20。
- 不支持 Wakeup Timer 的 F0：使用 Alarm + EXTI17。
- 不能把 F0 的实现简单复用 F1 的 `RTC_GetCounter/RTC_SetAlarm` 模型。

## P1 应该遵守

### P1-1 RTC 和备份域初始化要避免反复重置

依据：

- RM0008/RM0091/RM0360：RTC 属于备份域，时钟源选择和备份域复位会影响 RTC 状态。

规则：

- 启动时先判断备份域是否已有有效标志。
- 只有首次初始化或检测到时钟源失效时才重配备份域。
- Standby 唤醒后要通过 SBF/WUF/RTC flag 判断唤醒来源，不能当普通上电完全相同处理。

### P1-2 所有硬件等待必须有超时和降级路径

依据：

- AN2867、F1/F0 errata：LSE 启动在恶劣环境下可能失败或变慢。
- F0 ES0219：部分 IWDG 状态 flag 在 Stop 中可能不按普通预期清零。

需要设置超时的位置：

- LSE ready。
- LSI ready。
- RTC 同步。
- F1 RTC last task/RTOFF。
- F0 RTC init mode、WUTWF、ALRAWF。
- IWDG PVU/RVU。

超时后建议：

- LSE 超时：降级 LSI 或禁止低功耗。
- RTC 同步超时：禁止进入 Stop，返回 `LP_BLOCK_RTC_UNREADY` 或归入 `LP_BLOCK_IWDG_UNSAFE/LP_BLOCK_FAULT`。
- IWDG 状态超时：不要死等，退出低功耗准备流程。

### P1-3 调试态和量产态必须隔离验证

依据：

- F1 errata：Debugging Stop mode and SysTick timer。
- PM0215：调试操作可能造成伪唤醒。

规则：

- 低功耗电流和唤醒可靠性以脱离调试器的量产运行结果为准。
- 调试态允许开诊断日志，但不能改变量产睡眠入口条件。
- 若使用 DBGMCU 相关配置，必须在文档里记录，不可隐式依赖。

### P1-4 RTC 休眠时间要反馈给业务层

依据：

- PM0056/PM0215：Stop 中 SysTick 不可靠，软件 tick 会暂停。
- AN2604/AN4759：RTC 精度取决于 RTC 时钟源和校准。

规则：

- `LP_GetLastSleepSeconds()` 必须由 RTC 唤醒前后时间差或设定周期给出。
- SOC 静置、OCV 校准、保护延时、通信超时应使用该休眠时间做补偿。
- 若 RTC 源为 LSI，应标记时间精度等级，SOC 算法不能把它当高精度秒表。

### P1-5 LSE 硬件风险要进入项目审查清单

依据：

- AN2867：LSE 需要正确晶体、负载电容、PCB 走线和稳定性设计；资料中特别列出 LSE sensitivity to PC13 activity。
- F1 errata：LSE startup in harsh environments。

规则：

- 软件层面必须有 LSE timeout。
- 硬件层面检查 32.768 kHz 晶体、负载电容、走线长度、邻近高速/LED/按键信号。
- 若 PC13 用于 LED、按键或 Charlieplexing 高翻转，必须评估对 LSE 的影响。

## P2 后续增强

### P2-1 RTC 时间校准

依据：

- AN2604：F1 RTC calibration 可补偿晶体误差。
- AN4759/AN3371：新 RTC 支持更丰富的 calibration/alarms，具体能力依芯片而定。

建议：

- 第一版不做校准，只保证稳定睡眠和稳定唤醒。
- 后续可以测量 LSI/LSE 误差，用于 SOC 静置时间修正和 IWDG 安全裕量计算。

### P2-2 通信唤醒

依据：

- RM0091/RM0360：Stop 唤醒源和 USART/CAN 支持依具体型号而定。

建议：

- 第一版不做 CAN/USART Stop 唤醒。
- 后续若要做，必须单独评估波特率时钟恢复、首帧丢失、协议状态机恢复、总线电平唤醒和收发器电源控制。

### P2-3 Standby 深度策略

依据：

- RM0008/RM0091/RM0360：Standby 唤醒为 reset 流程，普通寄存器和 SRAM 丢失。

建议：

- 只给过放深度休眠、长期仓储或运输模式使用。
- 进入前持久化 SOC 快照、保护状态、MOS 策略、日志写完成标志和唤醒原因。
- 唤醒后必须走完整 boot 恢复流程，不复用 Stop 恢复流程。

## 后续代码设计前置结论

1. 需要独立 `bsp_rtc`，屏蔽 F1 counter/alarm 与 F0 calendar/wakeup 差异。
2. 需要独立 `bsp_clock`，提供 Stop 后系统时钟恢复。
3. 需要独立 `bsp_power`，统一 WFI/WFE、PWR flag、Stop/Standby 入口。
4. 需要 `app_lowpower` 维护状态机、禁止休眠位图、RTC 周期、IWDG 安全判断和业务层回调。
5. 所有禁止休眠原因必须可读，不能只在日志中临时打印。

