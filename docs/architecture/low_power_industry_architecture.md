# 低功耗固件通用架构与 BMS 保护板推荐架构

阶段：第一阶段只读分析。本文只总结架构原则与当前项目差异，不要求修改源码。

适用范围：STM32F0/F1 BMS 保护板，裸机主循环，标准外设库或寄存器实现，包含 AFE、SOC、保护、均衡、CAN、Modbus/RS485、Flash 参数/日志、LED 显示、IWDG、RTC 和任务调度。

## 1. 依据

### 1.1 官方资料原则

1. STM32F10xxx 低功耗模式不是单纯的“关 CPU”。
   ST AN2629 将 F1 低功耗分为 Sleep、Stop、Standby：Sleep 只关 CPU 时钟，外设和 SysTick 等仍运行；Stop 停止所有 1.8V 域时钟并关闭 PLL/HSI/HSE，但保留 SRAM 和寄存器；Standby 关闭 1.8V 域，SRAM 和大多数寄存器丢失，只保留 Backup 域和 Standby 电路。架构上应把 Stop 作为第一阶段主路径，把 Standby 作为单独的复位式策略，而不是直接替换 Stop。

2. Stop 唤醒后必须恢复系统时钟。
   AN2629 明确说明 F1 从 Stop 退出后系统时钟选择为 HSI RC，因此使用 HSE/PLL 的工程必须在唤醒后恢复 SYSCLK、AHB、APB、TIM、SysTick/Delay 等时基，再恢复依赖时钟的外设。

3. Stop 进入前必须清 pending。
   AN2629 说明进入 Stop 前所有 EXTI pending 和 RTC Alarm flag 必须复位，否则 Stop 进入流程会被忽略，程序继续执行。工程架构必须把“清唤醒 pending”放在统一入口，而不是散落在 RTC、GPIO、通信模块中。

4. RTC 是周期唤醒的主定时源。
   AN2629 说明 RTC 可在低功耗下提供周期唤醒；F1 使用 RTC Alarm 从 Stop 唤醒时，需要配置 EXTI Line 17 为上升沿并配置 RTC 产生 Alarm；从 Standby 由 RTC Alarm 唤醒时不需要配置 EXTI17。F0 系列 RM0091 具备 RTC Wakeup Timer/Alarm，通常可用 Wakeup Timer 做固定周期，Alarm 做绝对时间点。

5. LSE 优先，LSI 可容错但不适合长期精确计时。
   AN2629 推荐 RTC 可由 LSE 或 LSI 驱动，LSE 精度和低功耗更适合长期计时，LSI 省成本但误差大。BMS 中 SOC 静置时间、OCV 校准、老化剩余时间应优先依赖 LSE；LSI fallback 要记录状态并降低时间可信度。

6. IWDG 一旦启动不能靠软件停止。
   AN2629 明确 IWDG 由 Key register 或硬件选项启动后，除复位外不能停止。低功耗周期必须短于 IWDG 超时时间并在入睡前、唤醒后喂狗；若周期超过 IWDG，只能进入“允许 IWDG 复位”的 Standby/运输模式，不能套用 Stop 周期唤醒逻辑。

7. Flash 写入期间不应进入 Stop。
   AN2629 说明如果 Flash programming 正在进行，Stop 进入会延迟到 memory access 完成。虽然硬件会延迟，但 BMS 架构不能依赖这个隐式行为，因为 Flash 擦写/日志保存和参数更新往往伴随协议响应、状态更新、IWDG 喂狗和掉电一致性要求，应在业务层用 `LP_BLOCK_FLASH_BUSY` 显式禁止休眠。

8. ADC/DAC 进入 Stop 前要关。
   AN2629 指出 ADC/DAC 在 Stop 中可能继续耗电，除非进入前关闭。BMS 中 ADC、AFE CADC、采样电源、LED 扫描和通信收发器都应有明确的 before/after 钩子。

9. 低功耗管理应支持多模块投票。
   ST 的 Low-power manager Utility 采用多个 client 请求禁用 Stop/Standby，然后计算当前允许的最低功耗模式。这种“阻塞原因位图/投票”比单一 `if` 更适合 BMS，因为通信、Flash、AFE、保护、LED、IWDG 都会在不同时间阻塞休眠。

资料链接：
- ST AN2629, STM32F101xx/102xx/103xx low-power modes: https://www.st.com/resource/en/application_note/an2629-stm32f101xx-stm32f102xx-and-stm32f103xx-lowpower-modes-stmicroelectronics.pdf
- ST RM0091, STM32F0x1/F0x2/F0x8 reference manual: https://www.st.com/resource/en/reference_manual/rm0091-stm32f0x1stm32f0x2stm32f0x8-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
- ST RTC low-power wakeup article: https://community.st.com/t5/stm32-mcus/how-to-configure-the-rtc-to-wake-up-the-stm32-from-low-power/ta-p/49836
- ST Low-power manager Utility concept: https://wiki.st.com/stm32mcu/wiki/Connectivity%3ASTM32CubeWBA_Low_Power_Management

### 1.2 当前项目源码依据

当前工程是 STM32F10x 标准外设库项目，已经具备一个低功耗雏形：

- `103 + 309/Project/Source/rtc_sleep.h:27` 定义 `NORMAL_MODE / HICCUP_MODE / DEEP_MODE / NO_SLEEP`。
- `103 + 309/Project/Source/rtc_sleep.h:39` 定义 `LOW_POWER_RTC_BLOCK_CURRENT / MCU_WAKE / FACTORY_AGING / EXT_COMM / AFE_NOT_IDLE` 等阻塞原因。
- `103 + 309/Project/Source/rtc_sleep.c:147` 的 `low_power_select_sleep_mode()` 已按电压、电流、MCU_WAKE、老化、外部通信、AFE 空闲判断是否休眠。
- `103 + 309/Project/Source/rtc_sleep.c:303` 的 `rtc_sleep_run_hiccup_cycle()` 已形成“准备 RTC、进入 Stop、识别 RTC 唤醒、恢复外设、运行 CAN RTC 服务、累加 SOC 静置时间”的周期结构。
- `103 + 309/Project/Source/rtc_sleep_port.c:108` 的 `RtcSleep_PortPrepareRtcStop()` 已把保存状态、`Init_RTC()`、`IOstatus_RTCMode()`、`InitWakeUp_RTCMode()` 收敛在 MCU port。
- `103 + 309/Project/Source/rtc_sleep_port.c:118` 的 `RtcSleep_PortEnterStop()` 在 Stop 前后喂 IWDG。
- `103 + 309/Project/Source/rtc_sleep_port.c:131` 的 `RtcSleep_PortRestoreAfterStop()` 调用 `InitRunAfterStopWakeup()` 做恢复。
- `103 + 309/Project/Source/RTC.c:26`、`:41` 已提供带超时的 `RTC_WaitForLastTaskSafe()` 和 `RTC_WaitForSynchroSafe()`，避免官方库默认死等。
- `103 + 309/Project/Source/RTC.c:206` 的 `RTC_ClockConfig()` 已做 LSE 优先、失败后 LSI fallback。
- `103 + 309/Project/Source/RTC.c:408` 的 `RTC_WKTimeConfig()` 已在 Stop 前关闭秒中断和 Alarm、清 pending、设置下一次 Alarm。
- `103 + 309/Project/Source/conf/conf.c:129` 的 `LowPower_ClearWakeupPending()` 已统一清 EXTI0/9/12/13 等 pending。
- `103 + 309/Project/Source/conf/conf.c:374` 的 `Sys_StopMode()` 已停 TIM3、清 pending、执行 `PWR_EnterSTOPMode()`，并在返回后调用 `cpu_frequency_conf()`。
- `103 + 309/Project/Source/conf/conf.c:392` 的 `InitRunAfterStopWakeup()` 已恢复 Delay、RTC 中断、IO、ADC、LED、USART、CAN、TIM3、AFE IIC。
- `103 + 309/Project/Source/System_Init.c:33` 的 `Init_IWDG()` 在 RTC 功能开启时使用 `IWDG_Prescaler_256` 和 `Reload=0x0FFF`。
- `103 + 309/Project/Source/Flash.c:259`、`:456`、`:569` 等 Flash 保存路径会执行擦写和编程，当前更适合由低功耗管理器显式纳入 Flash busy 阻塞。
- `103 + 309/Project/Source/Sci_Upper.c:1679` 已有 `Sci_IsAnyPortBusy()`，可作为 Modbus/RS485 活跃阻塞依据。
- `103 + 309/Project/Source/Can_HDX.c:896` 已有 `Can_IsBusActive()`，`103 + 309/Project/Source/Can_HDX.c:906` 已有 `Can_RtcWakeService()`。

## 2. 嵌入式低功耗固件常见架构

### 2.1 推荐分层

建议分 4 层，避免低功耗逻辑直接散落在业务模块和外设驱动里：

| 层级 | 职责 | 典型接口 |
|---|---|---|
| App LowPower 策略层 | 状态机、阻塞原因、周期策略、睡眠模式选择 | `LP_Task()`、`LP_CanSleep()`、`LP_GetBlockReason()` |
| BMS 业务约束层 | 保护/MOS/充放电/SOC/老化/升级/Flash/通信活动判定 | `BmsLp_GetBlockReason()` |
| BSP/Port 适配层 | RTC、PWR、Clock、EXTI、GPIO、TIM、ADC、UART、CAN 的具体寄存器操作 | `BSP_RTC_SetWakeup()`、`BSP_PWR_EnterStop()`、`BSP_Clock_RestoreAfterStop()` |
| Driver/Module 层 | AFE、SOC、CAN、Modbus、Flash、LED 自身 before/after 钩子 | `Can_PrepareSleep()`、`ADC_StopForLowPower()` |

当前项目已经接近该分层：`rtc_sleep.c` 是策略层，`rtc_sleep_port.c` 是 Port 层，`RTC.c/conf.c` 是 BSP/底层层，CAN/SOC/AFE/LED/Flash 是业务和驱动层。下一步应做的是规范接口命名和补齐阻塞原因，而不是推翻现有结构。

### 2.2 通用状态机

低功耗不能只写成“空闲就 WFI/STOP”，应由显式状态机驱动：

| 状态 | 进入条件 | 主要动作 | 退出条件 |
|---|---|---|---|
| `LP_STATE_RUN` | 上电、唤醒、通信/保护/采样活跃 | 正常运行所有任务 | 周期调度到空闲检查 |
| `LP_STATE_IDLE_CHECK` | 主循环空闲窗口到达 | 收集阻塞原因，计算 sleep deadline | 有阻塞回 RUN；无阻塞进入 PREPARE |
| `LP_STATE_PREPARE_SLEEP` | 无阻塞且 IWDG 安全 | 保存业务状态，停 LED/TIM/ADC/CAN/USART，配置 RTC/EXTI，清 pending | 准备完成进入 STOP；准备失败进入 ERROR/RUN |
| `LP_STATE_STOP_SLEEP` | RTC/EXTI 已配置 | 喂 IWDG，进入 Stop | RTC/GPIO/复位唤醒 |
| `LP_STATE_WAKEUP_RESTORE` | Stop 返回 | 先恢复系统时钟，再恢复 Delay/SysTick/TIM/ADC/UART/CAN/LED/AFE，统计休眠时间 | 恢复完成回 RUN |
| `LP_STATE_DEEP_STANDBY` | 运输/过放深休眠策略明确 | 写 Backup/Flash 快照，配置 RTC 或 WKUP，进入复位式低功耗 | 复位启动路径恢复 |
| `LP_STATE_ERROR` | RTC/时钟/外设恢复失败 | 禁止继续休眠，记录错误，回 RUN 或触发安全复位 | 人工或策略恢复 |

关键约束：
- 只允许主循环或低优先级任务进入 Stop，不在中断里进入 Stop。
- 入睡前必须二次检查阻塞原因，避免刚收到通信帧或刚置位 Flash 保存请求时误睡。
- `BeforeSleep` 和 `AfterWakeup` 必须可重复调用，失败要有超时和错误上报。
- Stop 返回后先恢复系统时钟，再恢复所有依赖 APB/AHB 的外设。
- 唤醒源识别要同时看 RTC flag、EXTI pending、业务 GPIO 和 reset/backup 标志，不能只看一个中断标志。

### 2.3 阻塞原因位图

行业常见做法是每个模块提供自己的阻塞位，低功耗管理器做 OR 汇总：

| 阻塞位 | 触发模块 | 为什么必须阻塞 |
|---|---|---|
| `LP_BLOCK_CHARGE` | 充电检测、充电电流、充电器插入 | 充电时需要持续采样、保护、SOC 和通信 |
| `LP_BLOCK_DISCHARGE` | 放电电流、负载存在 | 负载存在时进入 Stop 可能丢保护响应或通信响应 |
| `LP_BLOCK_COMM` | CAN、Modbus、RS485、升级 | 防止半帧、待发送 ACK、总线探测期间睡眠 |
| `LP_BLOCK_KEY` | 按键/MCU_WAKE/显示唤醒 | 用户交互窗口内不能立刻回睡 |
| `LP_BLOCK_AFE_BUSY` | AFE 采样、CADC、EEPROM/MTP、故障读取 | 防止 AFE 状态和 MCU 状态不同步 |
| `LP_BLOCK_FLASH_BUSY` | Flash 擦写、日志、参数保存、SOC 快照 | 防止擦写中断、协议响应丢失、掉电一致性差 |
| `LP_BLOCK_UPGRADE` | IAP、CAN 升级、串口升级 | 升级窗口禁止低功耗 |
| `LP_BLOCK_FAULT` | 保护状态未稳定、短路/OCD/过温等 | 保护处理优先于省电 |
| `LP_BLOCK_LED_ACTIVE` | SOC 显示、老化显示、按键显示 | 显示窗口内需要 TIM/GPIO 扫描 |
| `LP_BLOCK_IWDG_UNSAFE` | RTC 周期 >= IWDG 安全窗口 | 防止 Stop 期间被 IWDG 误复位 |

当前项目已有 `LOW_POWER_RTC_BLOCK_CURRENT / MCU_WAKE / FACTORY_AGING / EXT_COMM / AFE_NOT_IDLE`，但还缺少显式的 Flash busy、Upgrade、Fault、LED active、IWDG unsafe。第一版设计可先补文档和接口，不立即改源码。

## 3. BMS 保护板低功耗推荐架构

### 3.1 目标排序

本项目第一阶段不追求最低电流，应按以下顺序优化：

1. 稳定睡眠：进入 Stop 前 pending 清干净，外设进入已知状态。
2. 稳定唤醒：RTC/GPIO 唤醒后必能回主循环。
3. 通信不乱：CAN/Modbus 活跃时禁止睡眠，待发送 ACK 和升级窗口不睡。
4. 保护不丢：AFE/MOS/保护状态未同步时不睡。
5. IWDG 不误复位：RTC 唤醒周期小于 IWDG 超时并留足恢复窗口。
6. SOC 时间不丢：休眠秒数进入 SOC 静置/OCV 校准节拍。
7. Flash 不被打断：参数、日志、SOC、老化进度保存期间不睡。

### 3.2 BMS 推荐运行流

推荐第一版只做 Stop + RTC 周期唤醒，不做 CAN/USART Stop 唤醒：

1. 主循环运行采样、保护、SOC、通信、Flash、LED。
2. `LP_Task()` 每 1s 或固定节拍检查 BMS 是否空闲。
3. 若充放电电流、通信、AFE、Flash、升级、故障、LED、IWDG 任一阻塞存在，记录 block reason 并保持 RUN。
4. 空闲达到阈值后进入 `PREPARE_SLEEP`。
5. 保存必要状态：CAN 状态、SOC 快照、老化进度、睡眠日志请求、RTC 起始计数。
6. 关闭或静默外设：LED 扫描、TIM3、ADC、CAN 收发器/控制器、USART、非唤醒 GPIO；保留必要 GPIO EXTI、RTC、IWDG、AFE 硬件保护。
7. 设置 RTC 周期 Alarm/Wakeup，清 RTC flag 和 EXTI pending。
8. 喂 IWDG，进入 Stop。
9. RTC 唤醒后先恢复时钟，再恢复 Delay/TIM/ADC/UART/CAN/LED/AFE。
10. 做短服务窗口：AFE 快速同步、保护状态读取、CAN 探测/广播、SOC 静置补偿。
11. 如果仍无阻塞，继续下一轮 RTC Stop；如果有外部唤醒或业务阻塞，退出低功耗回 RUN。

当前项目的 `rtc_sleep_run_hiccup_cycle()` 已经接近该流程：`RtcSleep_PortEnterStop()` 后读取 RTC elapsed，`RtcSleep_PortRestoreAfterStop()` 恢复，随后 `RtcSleep_PortRunCanRtcWakeService()` 和 `RtcSleep_PortApplySocRtcRest()` 处理业务。

### 3.3 Stop 与 Standby 分工

| 场景 | 推荐模式 | 原因 |
|---|---|---|
| 空闲但需要周期检测 BMS 状态 | Stop + RTC 周期唤醒 | SRAM 保留，恢复快，适合逐步改造 |
| 短时主循环空闲 | Sleep 或不处理 | 省电有限，容易引入调度复杂度，第一版不建议投入 |
| 长期运输/仓储 | Standby 或 AFE SHIP，单独设计 | 复位式唤醒，需 BootFlag/Backup/Flash 恢复 |
| 过放深休眠 | Standby/AFE SHIP 或 Stop 降级策略 | 优先保护电池，不能被普通通信和显示阻塞 |
| 通信或升级活跃 | Run | 防止协议半包、ACK 丢失、IAP 失败 |

当前项目有 `NORMAL_MODE / HICCUP_MODE / DEEP_MODE`，但 `DEEP_MODE` 是否真正等价 Standby/SHIP 需要由后续 BmsLogicAgent 和 RtcAgent 进一步确认。IndustryArchitectureAgent 的建议是：第一版只把 `HICCUP_MODE` 作为 Stop + RTC 周期唤醒主路径，暂不扩大 `DEEP_MODE` 行为。

### 3.4 外设休眠与恢复顺序

推荐顺序如下：

| 阶段 | 顺序 | 动作 |
|---|---:|---|
| BeforeSleep | 1 | 关闭通信发送入口，等待或丢弃安全可丢弃队列 |
| BeforeSleep | 2 | 如果 `Sci_IsAnyPortBusy()` 或 `Can_IsBusActive()` 为真，取消入睡 |
| BeforeSleep | 3 | 保存 SOC/老化/日志请求，确认 Flash 非 busy |
| BeforeSleep | 4 | LED 进入 sleep，停扫描 TIM，GPIO 进入低漏电状态 |
| BeforeSleep | 5 | ADC 停止转换，AFE 确认可睡或保持硬件保护 |
| BeforeSleep | 6 | CAN/USART DeInit 或收发器断电，保留第一版所需外部唤醒 GPIO |
| BeforeSleep | 7 | 配置 RTC Alarm/Wakeup，清 RTC/EXTI/NVIC pending |
| BeforeSleep | 8 | 喂 IWDG，进入 Stop |
| AfterWakeup | 1 | 恢复 HSE/PLL/SYSCLK/AHB/APB，更新 `SystemCoreClock` |
| AfterWakeup | 2 | 恢复 Delay/SysTick/TIM3 主时基 |
| AfterWakeup | 3 | 恢复 GPIO、ADC、AFE IIC、UART、CAN、LED |
| AfterWakeup | 4 | 读取 RTC elapsed 和唤醒源 |
| AfterWakeup | 5 | AFE/MOS/保护状态重新同步 |
| AfterWakeup | 6 | SOC 静置补偿、CAN RTC 服务、通信窗口 |

当前项目 `Sys_StopMode()` 已在 Stop 前停 TIM3，并在返回后调用 `cpu_frequency_conf()`；`InitRunAfterStopWakeup()` 已恢复 Delay、RTC、IO、ADC、LED、USART、CAN、TIM3、AFE IIC。建议后续设计文档把这个顺序固化为正式 `LP_BeforeSleep()` / `LP_AfterWakeup()` 合约，避免新增模块绕过恢复顺序。

## 4. 重点问题分析

### 4.1 状态机

推荐使用显式状态机而不是单个 `sleep()` 函数。原因：
- BMS 阻塞条件多，状态机便于记录“为什么没睡”。
- Stop 准备和唤醒恢复是跨模块事务，状态机便于失败回滚。
- RTC 周期服务可能连续睡眠多轮，状态机能区分“短 RTC 服务后继续睡”和“外部事件退出低功耗”。

当前项目 `rtc_sleep.c` 已有 `readyToSleep`、`mode`、`blockReason`、`elapsedSeconds`，是状态机雏形。建议后续迁移到用户给出的 `LP_STATE_RUN / IDLE_CHECK / PREPARE_SLEEP / STOP_SLEEP / WAKEUP_RESTORE / DEEP_STANDBY / ERROR`，但第一版可以保留现有 `HICCUP_MODE` 路径，逐步包一层新接口。

### 4.2 外设休眠恢复

行业推荐是“模块自报阻塞 + 平台统一执行顺序”：
- CAN/Modbus 不应在低功耗管理器里直接读写协议内部状态，而是通过 `Can_IsBusActive()`、`Sci_IsAnyPortBusy()`、`Can_PrepareSleep()` 这类接口表达。
- Flash 不应由低功耗管理器推断 `u8FlashUpdateFlag`、日志状态和 SOC 保存状态，应由 Storage/Flash 模块提供 `StorageFlash_IsBusy()` 或由写入包装层置位。
- LED 不应只看显示模式文字，应由 LedBar 模块提供 `LedBar_IsActiveForSleepBlock()`。
- AFE 不应在策略层直接操作寄存器，应继续沿用 `RtcSleep_AfePortIsSleepBlocked()` 这类 port 接口。

当前项目的 `rtc_sleep_port.c` 已采用 port 方式隔离 AFE/CAN/SOC/RTC/Clock，方向正确。主要缺口是 Flash、Sci、LED、IWDG 还没有统一进入阻塞原因位图。

### 4.3 RTC 周期唤醒

推荐策略：
- F1：Stop 周期唤醒用 RTC Alarm + EXTI17，设置 `RTC_SetAlarm(RTC_GetCounter() + seconds)`，进入前清 `RTC_FLAG_ALR`、`RTC_IT_ALR`、`EXTI_Line17` 和相关 NVIC pending。
- F0：优先用 RTC Wakeup Timer 做固定周期；如果项目需要绝对时间点，再使用 Alarm。
- 运行态秒中断和 Stop 唤醒中断分开：运行态可用秒中断更新时间；进入 Stop 前建议关闭秒中断，避免误判唤醒源。
- 唤醒后不要只靠 `RTC_GetITStatus(RTC_IT_ALR)` 判断，因为它依赖中断使能位；诊断应同时看 RTC flag、EXTI pending、软件 `is_rtc_wakekup` 和累计计数。

当前项目 `RTC_WKTimeConfig()` 已按上述方向处理秒中断、Alarm pending 和下一次 Alarm；后续重点是把该路径抽象成 `bsp_rtc.c/h`，并保留超时等待和 LSI fallback。

### 4.4 IWDG 兼容

推荐安全关系：

`RTC_WAKE_PERIOD + STOP_EXIT_RESTORE_TIME + SERVICE_WINDOW_TIME + CLOCK_TOLERANCE < IWDG_TIMEOUT * 0.7`

解释：
- `RTC_WAKE_PERIOD` 是 Stop 中睡眠秒数。
- `STOP_EXIT_RESTORE_TIME` 是 HSI/HSE/PLL、Delay、TIM、ADC、CAN、USART、AFE 恢复时间。
- `SERVICE_WINDOW_TIME` 是唤醒后 CAN 探测、AFE 同步、SOC 更新、日志处理时间。
- `CLOCK_TOLERANCE` 需要覆盖 LSI 误差和晶振启动异常。
- 乘 0.7 是工程安全系数，避免温度、LSI 漂移、偶发长阻塞导致误复位。

当前项目 `RtcSleep_PortEnterStop()` 在 Stop 前后喂狗，`Init_IWDG()` 在 RTC 模式下把 IWDG 配到最大 reload。建议后续由 `LP_BLOCK_IWDG_UNSAFE` 显式保护：如果 `LP_SetWakeupPeriod(seconds)` 超过安全窗口，禁止 Stop 或降级到复位式 Standby 策略。

### 4.5 通信禁止休眠

推荐规则：
- CAN 总线处于活跃、发送邮箱占用、TX 队列非空、RTC 唤醒探测窗口未结束、升级/IAP 命令窗口内，禁止 Stop。
- Modbus/RS485 任一端口 RX frame 未完成、TX 未完成、DE/RE 方向切换未完成、上位机写参数处理中，禁止 Stop。
- 第一版不做 CAN/USART Stop 唤醒，是合理选择；通信活跃时保持 Run，空闲后再 Stop。

当前项目已有 `Can_IsBusActive()` 和 `Sci_IsAnyPortBusy()`，建议把二者纳入 `LP_BLOCK_COMM`。已有 `RTC_ExtComCnt` 变化会触发 `LOW_POWER_RTC_BLOCK_EXT_COMM`，但它更像“最近有外部通信”的窗口，不等价于“当前协议栈忙”；二者都需要保留。

### 4.6 Flash 写入禁止休眠

推荐规则：
- Flash 擦写、编程、验证、日志合并、参数保存、SOC 快照、老化进度保存期间统一置 `LP_BLOCK_FLASH_BUSY`。
- Flash 写入请求已挂起但尚未执行，也应阻塞，避免刚入睡就丢保存窗口。
- Flash 写入完成后再允许低功耗，且需要预留一次协议 ACK/日志输出服务窗口。

当前项目 `StorageFlash_SaveSocData()`、`StorageFlash_SaveAfeData()`、`StorageFlash_SaveRwParamData()`、`StorageFlash_SaveLogData()`、`StorageFlash_SaveFactoryAgingData()` 都会进入 Flash 擦写/编程路径。当前低功耗策略层没有显式 Flash busy 阻塞位，后续最小实现应补一个统一 busy 计数或临界区标志。

## 5. 当前项目与推荐架构差异

| 项目 | 当前状态 | 推荐方向 |
|---|---|---|
| 状态机 | 有 `mode/readyToSleep/blockReason` 雏形 | 封装成明确 `LP_STATE_*`，保留现有 HICCUP 路径 |
| 阻塞原因 | 已有电流、MCU_WAKE、老化、外部通信、AFE | 增加 COMM busy、Flash busy、Upgrade、Fault、LED、IWDG unsafe |
| RTC 周期唤醒 | F1 RTC Alarm + EXTI17 方向正确 | 固化到 `bsp_rtc`，保留超时等待、LSE/LSI 策略 |
| 时钟恢复 | `cpu_frequency_conf()` + `InitRunAfterStopWakeup()` 已恢复 | 明确 Clock BSP 合约，先恢复 SYSCLK 再外设 |
| IWDG | Stop 前后喂狗，RTC 模式配置较长超时 | 增加安全窗口计算和阻塞位 |
| 通信 | CAN 有活跃状态，SCI 有 busy API，但低功耗未统一使用 | 统一映射为 `LP_BLOCK_COMM` |
| Flash | Flash 写入路径清晰，但低功耗未显式阻塞 | 增加 `StorageFlash_IsBusy()` 或写入临界区计数 |
| 外设恢复 | 已恢复 ADC/USART/CAN/TIM/LED/AFE IIC | 文档化顺序并用统一 `LP_AfterWakeup()` 表达 |
| Standby/Deep | 有 `DEEP_MODE` 名称和 reset sleep 流程 | 暂不扩大范围，作为后续单独设计 |

## 6. 最小可行架构建议

第一版建议只在设计层定义接口，不立即改源码：

```c
void LP_Init(void);
void LP_Task(void);
uint8_t LP_CanSleep(void);
uint32_t LP_GetBlockReason(void);
void LP_SetWakeupPeriod(uint32_t seconds);
void LP_EnterStop(uint32_t seconds);
void LP_BeforeSleep(void);
void LP_AfterWakeup(void);
uint32_t LP_GetLastSleepSeconds(void);
```

模块建议：
- `app_lowpower.c/h`：状态机、阻塞位图、策略。
- `bsp_rtc.c/h`：RTC clock、Alarm/Wakeup、EXTI、pending、elapsed。
- `bsp_power.c/h`：Stop/Standby entry、PWR flag、唤醒 pending。
- `bsp_clock.c/h`：Stop 后系统时钟恢复。

保持不动：
- 不改 CAN/Modbus 协议格式。
- 不改 SOC 算法本体。
- 不改 AFE 保护阈值和 MOS 控制策略。
- 不改 Flash 存储布局。
- 不改 LED 显示业务规则。

优先接入的阻塞来源：
1. `Can_IsBusActive()`、CAN TX 队列/升级窗口 -> `LP_BLOCK_COMM`。
2. `Sci_IsAnyPortBusy()` -> `LP_BLOCK_COMM`。
3. Flash 写入包装层 busy -> `LP_BLOCK_FLASH_BUSY`。
4. LedBar 显示活动 -> `LP_BLOCK_LED_ACTIVE`。
5. IWDG 周期检查 -> `LP_BLOCK_IWDG_UNSAFE`。
6. AFE port busy -> `LP_BLOCK_AFE_BUSY`。

## 7. 后续增强方向

1. 把现有 `rtc_sleep_port.c` 的接口逐步迁移为通用 `bsp_* + app_lowpower`，但不一次性重构。
2. 为 F0/F1 做编译期差异：
   - F1：RTC Alarm + EXTI17。
   - F0：RTC Wakeup Timer 优先，Alarm 作为绝对时间唤醒。
3. 建立睡眠诊断寄存器/上位机可读状态：当前状态、阻塞位、最近唤醒源、最近休眠秒数、RTC clock source、IWDG unsafe 次数。
4. Standby/SHIP 单独设计 BootFlag、Backup Register、Flash 快照和上电恢复路径。
5. 建立低功耗测试矩阵：RTC、IWDG、通信、Flash、AFE、SOC、LED、充电唤醒、过放深休眠逐项验证。

## 8. IndustryArchitectureAgent 结论

当前项目方向不是从零开始：已经有策略层、port 层、RTC Alarm、Stop、时钟/外设恢复和 CAN/SOC RTC 服务。最小优化不应推翻现有代码，而应围绕“统一状态机 + 阻塞位图 + before/after 合约 + Flash/通信/IWDG 显式阻塞”做收敛。

第一版主路径建议固定为 Stop + RTC 周期唤醒；Standby/SHIP、CAN/USART Stop 唤醒、最低电流优化都延后。这样最符合当前目标：先稳定睡眠、稳定唤醒、通信不乱、保护不丢、IWDG 不误复位。
