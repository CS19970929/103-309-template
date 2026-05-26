# RTC 低功耗最小可行架构设计

角色：Phase2ArchitectureAgent  
阶段：第二阶段设计方案  
日期：2026-05-26  
范围：`E:\TODO\103 + 309 - 副本` 当前 STM32F103C8 BMS 项目  
约束：本阶段只生成设计文档，不修改源码，不编译，不提交。

## 1. 设计目标

当前目标不是追求最低电流，而是先把 BMS 保护板必须具备的低功耗可靠性固定下来：

1. 稳定进入 Stop。
2. 稳定由 RTC 周期唤醒。
3. Stop 唤醒后恢复时钟和外设顺序确定。
4. 通信活跃时不睡，避免 Modbus/CAN 协议乱序。
5. Flash 擦写、参数保存、日志保存、SOC 快照、升级窗口不睡。
6. AFE、MOS、保护状态唤醒后重新同步。
7. RTC 周期必须满足 IWDG 安全窗口。
8. 保留当前 `HICCUP_MODE`、`NORMAL_MODE`、`DEEP_MODE` 业务语义，不大规模重构。

第一版明确只做：

- `Stop + RTC Alarm 周期唤醒`。
- 当前 STM32F103C8 使用 F1 的 `RTC Alarm + EXTI17 + RTCAlarm_IRQn`。
- 通信活跃时禁止休眠。
- 只把 CAN RTC 周期服务作为醒后服务窗口，不做 CAN Stop 唤醒。
- 不做 USART Stop 唤醒。
- 不把当前 `DEEP_MODE` 改成真正 STM32 Standby。

## 2. 官方依据

| 规则 | 官方依据 | 对本项目的约束 |
| --- | --- | --- |
| Stop 和 Standby 恢复模型不同，Stop 保持 SRAM/寄存器，Standby 是复位式恢复模型 | ST AN2629 `STM32F101xx, STM32F102xx and STM32F103xx low-power modes`，https://www.st.com/resource/en/application_note/an2629-stm32f101xx-stm32f102xx-and-stm32f103xx-lowpower-modes-stmicroelectronics.pdf | 第一版只把 `HICCUP_MODE` 做成运行态可恢复 Stop；当前 `DEEP_MODE` 不改成硬件 Standby。 |
| STM32F1 退出 Stop 后系统时钟由硬件选择为 HSI，HSE/PLL/SYSCLK 不会自动恢复 | ST RM0008 `STM32F101xx/102xx/103xx/105xx/107xx reference manual`，https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf | `LP_AfterWakeup()` 第一动作必须恢复系统时钟，再恢复 `SysTick/TIM/ADC/UART/CAN/LED/AFE`。 |
| STM32F1 RTC 是 counter/alarm 模型，Stop 下 RTC Alarm 唤醒需要 EXTI Line 17 | ST RM0008；ST AN2821 `Clock/calendar implementation on STM32F10xxx RTC`，https://www.st.com/resource/en/application_note/an2821-clockcalendar-implementation-on-the-stm32f10xxx-microcontroller-rtc-stmicroelectronics.pdf | 当前工程继续使用 `RTC_SetAlarm(RTC_GetCounter()+seconds)`、`EXTI_Line17`、`RTCAlarm_IRQn`。 |
| RTC 低功耗唤醒前要配置下一次唤醒并清相关 flag，所有等待点应避免无超时死等 | ST AN4759 `Introduction to using the hardware RTC and TAMP with STM32 MCUs`，https://www.st.com/resource/en/application_note/an4759-introduction-to-using-the-hardware-realtime-clock-rtc-and-the-tamper-management-unit-tamp-with-stm32-mcus-stmicroelectronics.pdf | 保留当前 `RTC_WaitForLastTaskSafe()`、`RTC_WaitForSynchroSafe()` 方向；后续 BSP 层统一检查返回值。 |
| IWDG 启动后除复位外不能停止，且由 LSI 运行；Stop 期间仍需考虑 IWDG 超时 | ST RM0008 IWDG/PWR 章节；ST WDG 官方资料 | RTC 周期必须小于 IWDG 最短超时并留恢复/服务裕量；超限置 `LP_BLOCK_IWDG_UNSAFE`。 |
| Flash programming/erase 与低功耗需要谨慎协调 | ST RM0008 Flash/PWR 规则；ST AN2629 低功耗模式选择原则 | Flash busy 或保存 pending 时禁止进入可恢复 Stop。 |

## 3. 当前项目依据

### 3.1 MCU、库和功能开关

- 当前 Keil 目标为 `STM32F103C8`，宏为 `STM32F10X_MD,USE_STDPERIPH_DRIVER`，见 `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx:17`、`:340`。
- 当前固定启用 F1 分支，见 `103 + 309/Project/Source/conf/conf_gpio.h:6-14`。
- 量产配置启用 IWDG 和 RTC，见 `103 + 309/Project/Source/conf/Project_Config.h:80-86`。
- 启动后调用 `Init_RTC()`，见 `103 + 309/Project/Source/AppInit.c:66-71`。

结论：第一版按 STM32F103C8 标准外设库工程设计，不引入 HAL，不引入 F0 Wakeup Timer 运行路径。

### 3.2 已有低功耗主链路

当前已经有可保留的主链路：

1. 主循环 `Runtime_RunOnce()` 调用 `App_LowPowerProcess()`，见 `103 + 309/Project/Source/Runtime.c:14-48`。
2. `App_LowPowerProcess()` 转到 `rtc_sleep()`，见 `103 + 309/Project/Source/rtc_sleep.c:231-234`。
3. `rtc_sleep()` 在 `HICCUP_MODE` 下循环调用 `rtc_sleep_run_hiccup_cycle()`，见 `rtc_sleep.c:414-465`。
4. `rtc_sleep_run_hiccup_cycle()` 调用准备、Stop、禁用唤醒、醒后恢复、SOC 补偿和 CAN RTC 服务，见 `rtc_sleep.c:303-345`。
5. `RtcSleep_PortPrepareRtcStop()` 调用 `LowPowerSleep_SaveCoreState()`、`Init_RTC()`、`IOstatus_RTCMode()`、`InitWakeUp_RTCMode()`，见 `103 + 309/Project/Source/rtc_sleep_port.c:108-116`。
6. `RtcSleep_PortEnterStop()` 调用 `Sys_StopMode()`，见 `rtc_sleep_port.c:118-123`。
7. `Sys_StopMode()` 进入 Stop 并在返回后调用 `cpu_frequency_conf()`，见 `103 + 309/Project/Source/conf/conf.c:374-385`。
8. `InitRunAfterStopWakeup()` 恢复 Delay、RTC、IO、ADC、USART、CAN、TIM3、AFE IIC，见 `conf.c:392-421`。

结论：最小可行架构不推翻该链路，而是在外层增加 `app_lowpower + bsp_rtc + bsp_power + bsp_clock` 的可复用边界。

### 3.3 现有 RTC 路径

- 当前 RTC 优先 LSE，失败回退 LSI，见 `103 + 309/Project/Source/RTC.c:206-278`。
- 当前已有带超时的 `RTC_WaitForLastTaskSafe()` 和 `RTC_WaitForSynchroSafe()`，见 `RTC.c:26-55`。
- 当前 Stop 唤醒配置入口是 `RTC_WKTimeConfig()`，见 `RTC.c:408-418`。
- 当前 F1 Alarm 设置在 `RTC_EnableAlarmAfterSeconds()`，使用 `RTC_GetCounter() + wake_seconds`，见 `RTC.c:305-317`。
- 当前 EXTI17/RTCAlarm IRQ 配置在 `RTC_AlarmConfig()`，见 `RTC.c:326-351`。
- 当前 Alarm IRQ 在 `RTCAlarm_IRQHandler()` 中清标志并置 `is_rtc_wakekup=true`，见 `RTC.c:492-520`。

结论：`bsp_rtc` 第一版只封装现有 F1 Alarm 能力，不改 RTC 驱动主体。

### 3.4 现有 Stop 和时钟恢复路径

- 当前系统时钟目标是 `SYSCLK_FREQ_HSE = HSE_VALUE`，非 72MHz PLL，见 `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/system_stm32f10x.c:106-116`。
- `cpu_frequency_conf()` 当前复用 `SystemInit()`、`SystemCoreClockUpdate()`、`InitDelay()`，见 `103 + 309/Project/Source/rtc_sleep_port.c:207-212`。
- `Sys_StopMode()` 在 `PWR_EnterSTOPMode()` 返回后立即调用 `cpu_frequency_conf()`，见 `103 + 309/Project/Source/conf/conf.c:374-385`。

结论：`bsp_clock` 第一版可以薄封装 `cpu_frequency_conf()`；长期再拆分出专用 `BspClock_RestoreAfterStop()`，避免低功耗框架长期直接依赖 `SystemInit()`。

## 4. 最小模块边界

### 4.1 `app_lowpower`

职责：

- 维护 `LP_STATE_*` 状态机。
- 汇总禁止休眠原因位图。
- 做 RTC 周期与 IWDG 安全判断。
- 做普通 Stop 与 deep request 的优先级仲裁。
- 固定 `LP_BeforeSleep()`、`LP_EnterStop()`、`LP_AfterWakeup()` 顺序。
- 对外暴露低功耗诊断：当前状态、阻塞位图、最近睡眠秒数、最近唤醒原因。

不直接做：

- 不直接操作 RTC 寄存器。
- 不直接调用 `PWR_EnterSTOPMode()`。
- 不直接重配 RCC。
- 不直接擦写 Flash。
- 不修改 CAN/Modbus 协议状态机。
- 不直接完整初始化 AFE。

第一版兼容策略：

- `LP_Task()` 可以先包装现有 `rtc_sleep()`，或者由现有 `App_LowPowerProcess()` 逐步转调 `LP_Task()`。
- 旧 `g_stLowPowerRtcStatus.blockReason` 保留为摘要字段；内部实际判断使用 `uint32_t` 位图。
- `HICCUP_MODE` 映射为 `LP_STATE_STOP_SLEEP` 主路径。
- `DEEP_MODE` 仍走现有 `SleepDeal_Continue()` 复位式睡眠，不映射成硬件 Standby。

建议接口：

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

### 4.2 `bsp_rtc`

职责：

- 屏蔽 STM32F1 与后续 STM32F0 的 RTC 唤醒实现差异。
- 第一版封装现有 F1 `RTC Alarm + EXTI17`。
- 统一 RTC 初始化结果、时钟源诊断、Alarm pending 清除、运行态秒中断恢复。
- 统一检查 RTC safe wait 返回值。

第一版封装关系：

| `bsp_rtc` 建议接口 | 当前可复用函数 | 当前依据 |
| --- | --- | --- |
| `BspRtc_Init()` | `Init_RTC()` | `RTC.c:437-480` |
| `BspRtc_SetStopAlarm(seconds)` | `RTC_WKTimeConfig()` 或内部拆到 `RTC_EnableAlarmAfterSeconds()` | `RTC.c:305-317`、`:408-418` |
| `BspRtc_ClearWakePending()` | `RTC_ClearAlarmPending()` | `RTC.c:281-288` |
| `BspRtc_DisableStopWakeup()` | `RTC_DisableStopWakeup()` | `RTC.c:420-425` |
| `BspRtc_RestoreRunInterrupts()` | `RTC_RestoreRunInterrupts()` | `RTC.c:427-435` |
| `BspRtc_IsRtcWake()` | `is_rtc_wakekup` 或 `RtcSleep_PortIsRtcWake()` | `rtc_sleep_port.c:156-159` |

F0 预留策略：

- `STM32F1`：`Alarm + EXTI17`。
- `STM32F0` 且芯片支持 Wakeup Timer：`Wakeup Timer + EXTI20`。
- `STM32F0` 不支持 Wakeup Timer 的型号：回退 `Alarm + EXTI17`。
- F0 分支只预留接口和能力宏，当前工程不实现、不编译。

### 4.3 `bsp_power`

职责：

- 提供唯一 Stop 入口。
- 固化进入 Stop 前的 PWR、EXTI、NVIC pending 合约。
- 隔离 `PWR_EnterSTOPMode()`，禁止业务层直接调用。
- 后续预留 Standby 入口，但第一版不使用。

第一版封装关系：

| `bsp_power` 建议接口 | 当前可复用函数 | 当前依据 |
| --- | --- | --- |
| `BspPower_EnterStop()` | `Sys_StopMode()` | `conf.c:374-385` |
| `BspPower_ClearWakePending()` | `LowPower_ClearWakeupPending()` | `conf.c:129-149` |
| `BspPower_ConfigWakeSourcesRtcMode()` | `InitWakeUp_RTCMode()` | `conf.c:268-272` |

兼容注意：

- 当前 `InitWakeUp_RTCMode()` 会复用 `InitWakeUp_NormalMode()`，因此带有普通外部唤醒源。第一版设计原则是“通信活跃禁止睡眠，不主动做 CAN/USART Stop 唤醒”；第三阶段如发现 UART EXTI 被动唤醒干扰验证，应把唤醒源配置显式化，但不改协议。
- `BspPower_EnterStandby()` 只预留接口，不接入当前 `DEEP_MODE`。

### 4.4 `bsp_clock`

职责：

- 提供 Stop 返回后的系统时钟恢复入口。
- 固定恢复顺序：先 SYSCLK/AHB/APB，再 Delay/SysTick，再外设。
- 后续支持不同 F0/F1 工程的 HSE/PLL 配置差异。

第一版封装关系：

| `bsp_clock` 建议接口 | 当前可复用函数 | 当前依据 |
| --- | --- | --- |
| `BspClock_RestoreAfterStop()` | `cpu_frequency_conf()` | `rtc_sleep_port.c:207-212` |
| `BspClock_GetSystemCoreClock()` | `SystemCoreClock` / `SystemCoreClockUpdate()` | `system_stm32f10x.c` |

兼容注意：

- 当前工程源码目标为 HSE 直驱，`SYSCLK_FREQ_72MHz` 被注释；第一版不切换 PLL、不改 CAN/USART 时钟参数。
- Keil `<Cpu>` 字段出现 `CLOCK(12000000)`，源码 `HSE_VALUE` 默认 8MHz。该问题作为 P1/P2 硬件确认项，不在第二阶段改代码。

## 5. 与现有 `rtc_sleep/RTC/conf` 的兼容策略

### 5.1 保留现有主路径

第一版不重写 `rtc_sleep_run_hiccup_cycle()`。建议第三阶段按以下顺序渐进：

1. 新增四个模块空壳和接口，不改变行为。
2. `app_lowpower` 先作为薄封装调用现有 `rtc_sleep()`。
3. `bsp_rtc` 薄封装 `RTC.c` 已有函数。
4. `bsp_power` 薄封装 `Sys_StopMode()`。
5. `bsp_clock` 薄封装 `cpu_frequency_conf()`。
6. 再逐步把阻塞位图和 IWDG 安全判断接入 `LP_CanSleep()`。

这样可以保持当前可运行链路不被一次性推翻。

### 5.2 旧状态到新状态映射

| 当前状态/函数 | 新框架语义 | 第一版处理 |
| --- | --- | --- |
| `NO_SLEEP` | `LP_STATE_RUN`，存在阻塞或无需睡眠 | 保留 |
| `HICCUP_MODE` | `LP_STATE_STOP_SLEEP`，Stop + RTC 周期唤醒 | 第一版主路径 |
| `NORMAL_MODE` | 复位式普通睡眠 | 不并入运行态 Stop 状态机 |
| `DEEP_MODE` | 当前复位式深度睡眠，不是硬件 Standby | 保留现状，不改 Standby |
| `rtc_sleep_run_hiccup_cycle()` | Stop 周期睡眠一次循环 | 初期继续复用 |
| `RTC_WKTimeConfig()` | 设置下一次 RTC Alarm | 由 `bsp_rtc` 包装 |
| `Sys_StopMode()` | 唯一 Stop 入口 | 由 `bsp_power` 包装 |
| `InitRunAfterStopWakeup()` | 外设恢复入口 | 由 `LP_AfterWakeup()` 固化调用顺序 |

### 5.3 不兼容点的处理原则

- 当前 `blockReason` 是单值枚举，新框架使用位图；短期保留旧字段作为摘要，不改变上位机和调试读取。
- 当前 IWDG 安全裁剪在 `RTC_GetWakeupPeriodSeconds()` 中被注释；新框架把该规则上移到 `app_lowpower`，超限返回 `LP_BLOCK_IWDG_UNSAFE`。
- 当前 `DEEP_MODE` 名称容易被误解为硬件 Standby；文档和新接口必须明确它仍是复位式睡眠。
- 当前通信 Stop 唤醒路径不作为第一版目标；若现有外部 EXTI 被保留，仅作为唤醒事件，不作为“通信可在 Stop 中可靠收发”的承诺。

## 6. 状态机设计

第一版状态机只驱动普通 RTC Stop 路径，deep 路径保留当前实现。

```text
LP_STATE_RUN
  -> LP_STATE_IDLE_CHECK
  -> LP_STATE_PREPARE_SLEEP
  -> LP_STATE_STOP_SLEEP
  -> LP_STATE_WAKEUP_RESTORE
  -> LP_STATE_RUN

LP_STATE_IDLE_CHECK
  -> LP_STATE_DEEP_STANDBY  仅表示 deep request 分支，第一版仍调用当前 DEEP_MODE 复位式流程

任一状态
  -> LP_STATE_ERROR         RTC/时钟/AFE/Flash 等关键失败
```

| 状态 | 进入条件 | 动作 | 退出条件 |
| --- | --- | --- | --- |
| `LP_STATE_RUN` | 上电、唤醒恢复完成、存在阻塞 | 正常运行 AFE、保护、SOC、通信、Flash、LED、IWDG | 1 秒节拍到达，进入 idle 评估 |
| `LP_STATE_IDLE_CHECK` | 低功耗评估节拍 | 汇总阻塞位图；判断 deep request；累计 idle 秒数 | 有阻塞回 RUN；idle 达标进 PREPARE；deep 达标走旧 deep 流程 |
| `LP_STATE_PREPARE_SLEEP` | 无普通 Stop 阻塞 | 二次采集阻塞位图；保存核心状态；准备 RTC/IO/外设 | 仍无阻塞进 STOP；出现阻塞回 RUN |
| `LP_STATE_STOP_SLEEP` | RTC Alarm 已配置，pending 已清 | 喂狗，进入 Stop | RTC Alarm 或允许的外部唤醒源唤醒 |
| `LP_STATE_WAKEUP_RESTORE` | Stop 返回 | 先恢复时钟，再恢复外设，再同步 AFE/MOS/保护，最后做 SOC RTC 补偿 | 正常回 RUN 或继续下一轮 Stop；异常进 ERROR |
| `LP_STATE_DEEP_STANDBY` | 低压、上位机强制、充电移除等 deep request | 第一版仅作为设计状态，实际调用现有 `DEEP_MODE` 复位式路径 | 由现有 `SleepDeal_Continue()` 和 `IsSleepStartUp()` 处理 |
| `LP_STATE_ERROR` | RTC 未就绪、时钟恢复失败、AFE 同步失败、IWDG 不安全等 | 禁止继续 Stop，保留诊断 | 错误清除或复位后回 RUN |

## 7. 禁止休眠原因位图

第一版内部使用 `uint32_t` 位图，至少包含：

```c
#define LP_BLOCK_CHARGE       (1UL << 0)
#define LP_BLOCK_DISCHARGE    (1UL << 1)
#define LP_BLOCK_COMM         (1UL << 2)
#define LP_BLOCK_KEY          (1UL << 3)
#define LP_BLOCK_AFE_BUSY     (1UL << 4)
#define LP_BLOCK_FLASH_BUSY   (1UL << 5)
#define LP_BLOCK_UPGRADE      (1UL << 6)
#define LP_BLOCK_FAULT        (1UL << 7)
#define LP_BLOCK_LED_ACTIVE   (1UL << 8)
#define LP_BLOCK_IWDG_UNSAFE  (1UL << 9)
```

当前项目信号映射：

| 位 | 当前可用依据 | 第一版策略 |
| --- | --- | --- |
| `LP_BLOCK_CHARGE` | `u16Ichg`，`RtcSleep_PortGetChargeCurrentMa()`，`GPIO_CHG_IN` 相关充电检测 | 禁止普通 Stop；充电输入有效时不进入 deep |
| `LP_BLOCK_DISCHARGE` | `u16IDischg`，`RtcSleep_PortGetDischargeCurrentMa()` | 带载禁止普通 Stop |
| `LP_BLOCK_COMM` | `Sci_IsAnyPortBusy()` 见 `Sci_Upper.c:1678-1690`；`Can_IsBusy()` 见 `Can_HDX.c:865-880` | 任一通信 busy 或静默窗口未满足时禁止 Stop |
| `LP_BLOCK_KEY` | `RtcSleep_PortIsMcuWakeActive()`、按键/MCU_WAKE 逻辑 | 用户交互窗口禁止普通 Stop |
| `LP_BLOCK_AFE_BUSY` | `RtcSleep_AfePortIsSleepBlocked()` 见 `rtc_sleep_afe_sh367309.c:11-28` | AFE 状态非空闲禁止普通 Stop |
| `LP_BLOCK_FLASH_BUSY` | `StorageFlash_Save*()`、`LogEvent_EEPROM()`、SOC/老化保存路径 | 第三阶段新增 busy/pending 适配前，不允许用猜测替代 |
| `LP_BLOCK_UPGRADE` | `u8FlashUpdateE2PROM/u8FlashUpdateFlag`、`App_FlashUpdate()` | 升级 pending 优先，不睡 |
| `LP_BLOCK_FAULT` | `Fault_ChangeToMCU()`、`SystemRuntime_SetMosStatus()` | 保护或 MOS 异常退出普通 Stop |
| `LP_BLOCK_LED_ACTIVE` | `APP_LedBar()`、`LedBar_SetSleep()`、休眠 SOC 显示 | 显示窗口内延迟 Stop |
| `LP_BLOCK_IWDG_UNSAFE` | `Init_IWDG()` 配置和 RTC 周期 | 非复位 Stop 周期超限时禁止 Stop |

旧 `LOW_POWER_RTC_BLOCK_*` 映射只做兼容显示，实际入睡判断必须使用完整位图。

## 8. Stop 周期与 IWDG 安全规则

当前 `Init_IWDG()` 在 RTC 功能开启时配置 `IWDG_Prescaler_256` 和 `Reload=0x0FFF`，见 `103 + 309/Project/Source/System_Init.c:33-48`。按 LSI 标称 40kHz 约 26.2s；按未校准 LSI 最快 60kHz 保守估算，最短约 17.5s。

第一版规则：

1. 已启用 IWDG 且走非复位式 Stop 时，RTC 周期必须小于 IWDG 最短超时。
2. 需要预留时钟恢复、外设恢复、CAN RTC 服务、AFE 同步、主循环喂狗余量。
3. 第一版建议默认允许周期不超过 10s。
4. 当前 `FEIDAO_CAN_RTC_PERIOD_SECONDS=1s` 保持不变。
5. 超限时不静默裁剪，而是置 `LP_BLOCK_IWDG_UNSAFE` 并禁止 Stop。
6. 若未来确需长于 IWDG 的休眠，应设计复位式 Standby/SHIP 策略，不在运行态 Stop 中赌博。

## 9. 睡前、Stop、醒后顺序

### 9.1 `LP_CanSleep()`

建议判定顺序：

1. 采集电流、充电 GPIO、MCU_WAKE、通信、AFE、Flash、升级、故障、LED、IWDG 周期快照。
2. 先识别 deep request，但不把 deep request 混入普通 block。
3. 无 deep request 时，任一普通 block 存在则禁止 Stop。
4. 进入 `LP_STATE_PREPARE_SLEEP` 后再二次执行一次，防止刚收到 Modbus/CAN 帧或刚置位 Flash 保存请求。

### 9.2 `LP_BeforeSleep()`

推荐顺序：

1. 二次 `LP_CanSleep()`。
2. 保存核心状态：`LowPowerSleep_SaveCoreState()`。
3. 确认通信静默，禁止在 busy 时调用会清队列的 `Can_PrepareSleep()`。
4. LED 收口：`LedBar_SetSleep()`、`LedBar_PrepareForStop()`。
5. ADC/TIM2/DMA 收口：`ADC_StopForLowPower()`。
6. CAN 收发器和通信电源按现有策略收口。
7. GPIO 进入 RTC 低功耗态：`IOstatus_RTCMode()`。
8. RTC 配置下一次 Alarm。
9. 清 RTC/EXTI/NVIC pending。
10. 喂 IWDG。

### 9.3 `LP_EnterStop(seconds)`

第一版由 `bsp_power` 封装现有 `Sys_StopMode()`：

1. 使能 PWR 时钟。
2. 关闭 TIM3 和 pending。
3. 清唤醒 pending。
4. `PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI)`。
5. 从 WFI 返回后立即恢复系统时钟。

### 9.4 `LP_AfterWakeup()`

推荐顺序：

1. `BspClock_RestoreAfterStop()`，当前封装 `cpu_frequency_conf()`。
2. `InitDelay()`，确保阻塞延时参数匹配恢复后的 `SystemCoreClock`。
3. `BspRtc_RestoreRunInterrupts()`，恢复 RTC 运行态秒中断。
4. `InitIO_rtc()`，恢复运行态 IO。
5. `ADC_StopForLowPower()` + `InitADC()`。
6. `USART_DeInit()` + `AppInit_InitSci()`。
7. `InitCan()`。
8. `InitTimer()`，恢复 TIM3 10ms 主时基。
9. `initAFE1_IIC()`。
10. 读取 AFE BSTATUS/MOS/Fault，同步 `SystemRuntime_SetMosStatus()` 和 `Fault_ChangeToMCU()`。
11. 用 `LP_GetLastSleepSeconds()` 或现有累计秒数执行 `SOC_ApplyRtcRelaxationCompensation()`。

当前 `InitRunAfterStopWakeup()` 已覆盖大部分动作，见 `103 + 309/Project/Source/conf/conf.c:392-421`。新框架第一版应把该顺序文档化并薄封装，不重排。

## 10. 第一版不做的内容

1. 不做 CAN Stop 唤醒。当前 `Can_RtcWakeService()` 只作为 RTC 周期唤醒后的服务窗口，见 `103 + 309/Project/Source/Can_HDX.c:906-933`。
2. 不做 USART Stop 唤醒。通信活跃作为 `LP_BLOCK_COMM`，不把 Stop 中收发作为可靠协议能力。
3. 不把 `DEEP_MODE` 改成硬件 Standby。当前 deep 路径是 `SleepDeal_Continue()` 写 BKP、AFE sleep、`MCU_RESET()` 后由 `IsSleepStartUp()` 进入 Stop，见 `103 + 309/Project/Source/SleepDeal.c:83-230`。
4. 不改 CAN/Modbus/RS485 协议、帧 ID、寄存器映射。
5. 不改 SOC 算法、OCV 表、Flash 存储布局。
6. 不在 Stop 唤醒后无条件完整 `InitAFE1()`。
7. 不切换到 72MHz PLL，不改 CAN/USART 时钟参数。
8. 不做最低电流极限优化。
9. 不大规模重排 `Runtime_RunOnce()` 任务。

## 11. 第三阶段建议修改文件清单

第三阶段经确认后再允许修改代码。建议先新增：

- `103 + 309/Project/Source/app_lowpower.c`
- `103 + 309/Project/Source/app_lowpower.h`
- `103 + 309/Project/Source/bsp_rtc.c`
- `103 + 309/Project/Source/bsp_rtc.h`
- `103 + 309/Project/Source/bsp_power.c`
- `103 + 309/Project/Source/bsp_power.h`
- `103 + 309/Project/Source/bsp_clock.c`
- `103 + 309/Project/Source/bsp_clock.h`

后续小步修改：

| 文件 | 目的 |
| --- | --- |
| `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx` | 加入新增 `.c` 文件 |
| `103 + 309/Project/Source/Runtime.c` 或 `rtc_sleep.c` | 将低功耗入口逐步切到 `LP_Task()` |
| `103 + 309/Project/Source/RTC.c/.h` | 通过 `bsp_rtc` 包装，检查 safe wait 返回值 |
| `103 + 309/Project/Source/conf/conf.c/.h` | 通过 `bsp_power/bsp_clock` 包装 Stop 和时钟恢复 |
| `103 + 309/Project/Source/Can_HDX.c/.h` | 接入 `Can_IsBusy()` 和 CAN 静默窗口 |
| `103 + 309/Project/Source/Sci_Upper.c/.h` | 接入 `Sci_IsAnyPortBusy()` 和串口静默窗口 |
| `103 + 309/Project/Source/Flash.c/.h` | 增加 Flash busy/pending 查询或 busy 计数 |
| `103 + 309/Project/Source/LedBar.c/.h` | 增加 LED active 阻塞查询 |
| `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c` | 接入 AFE/Fault 位图，不改变 AFE 保护策略 |

## 12. 验收标准

第三阶段最小实现完成后，至少满足：

1. Release 构建通过。
2. 量产配置保持 `PROJECT_CFG_BUILD_PROFILE=0`、`PROJECT_CFG_WDOG_ENABLE=1`、`PROJECT_CFG_RTC_ENABLE=1`。
3. 空闲时可进入 Stop + RTC Alarm 周期唤醒。
4. IWDG 不误复位。
5. Modbus/CAN 活跃时不进入 Stop。
6. Flash busy、升级 pending 时不进入 Stop。
7. AFE/MOS/Fault 异常时不进入普通 Stop。
8. 唤醒后 Modbus、CAN、ADC、TIM3、LED、AFE IIC 恢复。
9. SOC 能获得休眠秒数并执行静置补偿。
10. 不破坏当前 `DEEP_MODE`、CAN/Modbus 协议、SOC、保护、AFE、Flash、LED 业务行为。

烧录验证必须继续遵守仓库安全规则：

```powershell
.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash
```

禁止把 App bin 裸写到 `0x08000000`。

## 13. 结论

当前项目已经具备 STM32F103C8 的低功耗基础链路：`rtc_sleep.c` 做策略，`RTC.c` 做 F1 Alarm/EXTI17，`conf.c` 做 Stop 和醒后恢复，`rtc_sleep_port.c` 做业务适配。第二阶段最小架构不应推倒重写，而应在现有链路外层收敛出四个可移植边界：

- `app_lowpower`：状态机、阻塞位图、IWDG 安全、业务准入。
- `bsp_rtc`：F1 Alarm/EXTI17 封装，并预留 F0 能力分支。
- `bsp_power`：唯一 Stop/未来 Standby 入口和 pending 合约。
- `bsp_clock`：Stop 后系统时钟恢复入口。

第一版只做 `Stop + RTC Alarm 周期唤醒`，不做 CAN/USART Stop 唤醒，不改 `DEEP_MODE` 为 Standby。这样能以最小改动优先解决稳定睡眠、稳定唤醒、通信不乱、保护不丢、IWDG 不误复位的问题，并为后续跨 STM32F0/F1 项目复用保留接口。
