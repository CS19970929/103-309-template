# RTC 低功耗最终汇总报告

阶段：第一阶段只读分析 + 第二阶段设计方案汇总  
日期：2026-05-26  
范围：`E:\TODO\103 + 309 - 副本` 当前 STM32 BMS 项目  
约束：本报告只汇总已生成 subagent 文档和源码引用；未修改源码，未编译，未提交。

## 1. 输入资料

本报告汇总以下文档：

- `docs/research/stm32_low_power_research.md`
- `docs/research/rtc_stop_standby_rules.md`
- `docs/architecture/low_power_industry_architecture.md`
- `docs/current/low_power_current_usage.md`
- `docs/current/mcu_resource_related_to_low_power.md`
- `docs/current/rtc_usage_analysis.md`
- `docs/current/clock_usage_analysis.md`
- `docs/current/iwdg_usage_analysis.md`
- `docs/current/peripheral_sleep_analysis.md`
- `docs/design/bms_low_power_state_machine.md`
- `docs/design/low_power_block_reason.md`
- `docs/design/rtc_wakeup_design.md`
- `docs/design/clock_restore_after_stop.md`
- `docs/design/iwdg_low_power_strategy.md`
- `docs/design/peripheral_sleep_resume_plan.md`
- `docs/risk/low_power_risk_list.md`
- `docs/test/low_power_test_matrix.md`
- `docs/test/low_power_manual_test_steps.md`

## 2. 官方资料结论摘要

| 结论 | 官方依据 | 对本项目的约束 |
|---|---|---|
| 第一版应做 Stop + RTC 周期唤醒，不应直接追求 Standby 最低电流 | ST RM0008/RM0091/RM0360、PM0056/PM0215、AN2629 | 普通空闲休眠使用 Stop；Standby/复位式深睡单独设计，先不改当前 `DEEP_MODE` 语义。 |
| STM32F1 Stop 唤醒后系统时钟会回到 HSI | ST RM0008、AN2629、AN2821 | Stop 返回后必须先恢复 HSE/PLL/SYSCLK/AHB/APB，再恢复 SysTick/TIM/ADC/UART/CAN/LED。 |
| STM32F1 RTC Stop 唤醒固定走 RTC Alarm + EXTI17 | ST RM0008、AN2629、AN2821 | 当前 F103 项目继续使用 `RTC_SetAlarm(RTC_GetCounter()+seconds)`、`EXTI_Line17`、`RTCAlarm_IRQn`。 |
| STM32F0 RTC 不能照搬 F1 模型 | ST RM0091/RM0360、AN3371、AN4759 | 后续可移植框架需要区分 F0 Wakeup Timer + EXTI20 与 Alarm + EXTI17；部分 F030 不支持 periodic wakeup。 |
| RTC 低功耗时钟源应优先 LSE，允许 LSI 兜底 | ST AN2867、AN2604、AN4759 | LSE ready 等待必须有超时；LSI fallback 要记录诊断，SOC 静置时间不能默认高精度。 |
| IWDG 在 Stop/Standby 中仍运行 | ST RM0008/RM0091/RM0360、ST WDG 资料 | RTC 周期必须小于 IWDG 最短超时，并预留时钟恢复和服务窗口；超限时置 `LP_BLOCK_IWDG_UNSAFE`。 |
| 进入 Stop 前必须清 RTC/EXTI/NVIC pending | ST RM0008/RM0091/RM0360、PM0215 | `LP_BeforeSleep()` 必须固定清 pending 顺序，禁止业务模块直接调用 `PWR_EnterSTOPMode()`。 |
| Flash programming 期间不应进入 Stop | ST AN2629/RM0008 | Flash 擦写、日志、参数、SOC 快照、老化进度保存必须纳入 `LP_BLOCK_FLASH_BUSY`。 |

## 3. 当前项目低功耗相关代码位置

### 3.1 MCU、启动和调度

| 位置 | 函数/内容 | 结论 |
|---|---|---|
| `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx:17`、`:340` | `Device=STM32F103C8`，宏 `STM32F10X_MD,USE_STDPERIPH_DRIVER` | 当前工程是 STM32F1 标准外设库工程，不是 F0/HAL。 |
| `103 + 309/Project/Source/conf/conf_gpio.h:6-14` | `__STM32F1__` | 当前 GPIO/外设分支按 F1 编译。 |
| `103 + 309/Project/Source/main.c:5-12` | `main()` | `AppInit_Boot()` 后循环执行 `Runtime_RunOnce()`。 |
| `103 + 309/Project/Source/Runtime.c:14-48` | 主循环任务顺序 | 低功耗任务在 `App_Can()`、Flash 更新、日志任务之前执行，后续必须显式检查通信/Flash/升级 busy。 |
| `103 + 309/Project/Source/AppInit.c:7-54`、`:66-71` | `AppInit_InitDevice()`、`AppInit_Boot()` | 初始化包含 `IsSleepStartUp()`、外设、IWDG，随后运行态调用 `Init_RTC()`。 |

### 3.2 当前低功耗主路径

| 位置 | 函数/内容 | 结论 |
|---|---|---|
| `103 + 309/Project/Source/rtc_sleep.h:27-60` | `NORMAL_MODE/HICCUP_MODE/DEEP_MODE/NO_SLEEP`、`g_stLowPowerRtcStatus` | 已有状态雏形，但不是显式 `LP_STATE_*`，阻塞原因是单值枚举。 |
| `103 + 309/Project/Source/rtc_sleep.c:130-229` | `low_power_select_sleep_mode()` | 已按电流、MCU_WAKE、老化、外部通信计数、AFE 空闲判断是否休眠。 |
| `103 + 309/Project/Source/rtc_sleep.c:303-345` | `rtc_sleep_run_hiccup_cycle()` | 当前非复位式 Stop + RTC 周期唤醒主路径，包含 Stop、恢复、SOC 补偿、CAN RTC 服务。 |
| `103 + 309/Project/Source/rtc_sleep.c:414-465` | `rtc_sleep()` | 每 1 秒检查并进入 `HICCUP_MODE` 连续 Stop。 |
| `103 + 309/Project/Source/rtc_sleep_port.c:108-123` | `RtcSleep_PortPrepareRtcStop()`、`RtcSleep_PortEnterStop()` | 睡前保存核心状态，配置 RTC/IO/唤醒源，Stop 前后喂狗。 |
| `103 + 309/Project/Source/rtc_sleep_port.c:131-178` | `RtcSleep_PortRestoreAfterStop()`、SOC 补偿 | 唤醒后调用 `InitRunAfterStopWakeup()`，并把 RTC 休眠秒数给 SOC。 |
| `103 + 309/Project/Source/SleepDeal.c:83-230` | `SleepDeal_Continue()`、`IsSleepStartUp()` | `NORMAL_MODE/DEEP_MODE` 是写 BKP 标志、AFE sleep、MCU reset 后再 Stop 的复位式策略，不是硬件 Standby。 |

### 3.3 RTC、Stop、时钟和 IWDG

| 位置 | 函数/内容 | 结论 |
|---|---|---|
| `103 + 309/Project/Source/RTC.c:26-55` | `RTC_WaitForLastTaskSafe()`、`RTC_WaitForSynchroSafe()` | 已用超时等待规避标准库死等，但部分调用点未检查返回值。 |
| `103 + 309/Project/Source/RTC.c:206-278` | `RTC_ClockConfig()` | LSE 优先，失败后 LSI fallback；但 `BKP_DeInit()` 与业务 BKP 复用存在冲突风险。 |
| `103 + 309/Project/Source/RTC.c:281-317` | `RTC_ClearAlarmPending()`、`RTC_EnableAlarmAfterSeconds()` | 已清 `RTC_IT_ALR/RTC_FLAG_ALR/EXTI_Line17/RTCAlarm_IRQn` 并设置 Alarm。 |
| `103 + 309/Project/Source/RTC.c:326-351` | `RTC_AlarmConfig()` | 当前 F1 使用 EXTI17 + `RTCAlarm_IRQn`。 |
| `103 + 309/Project/Source/RTC.c:366-399` | `RTC_GetWakeupPeriodSeconds()` | 当前周期来自 CAN，IWDG 安全裁剪代码已被注释。 |
| `103 + 309/Project/Source/RTC.c:407-435` | `RTC_WKTimeConfig()`、`RTC_RestoreRunInterrupts()` | Stop 前关闭秒中断并设置 Alarm，醒后恢复运行态秒中断。 |
| `103 + 309/Project/Source/RTC.c:492-535` | `RTCAlarm_IRQHandler()`、`RTC_IRQHandler()` | Alarm 唤醒标记 `is_rtc_wakekup=true`，秒中断仅置更新时间标志。 |
| `103 + 309/Project/Source/conf/conf.c:374-385` | `Sys_StopMode()` | Stop 前停 TIM3、清 pending、进入 Stop，返回后调用 `cpu_frequency_conf()`。 |
| `103 + 309/Project/Source/conf/conf.c:392-421` | `InitRunAfterStopWakeup()` | 恢复 Delay、RTC、IO、ADC、LED、USART、CAN、TIM3、AFE IIC。 |
| `103 + 309/Project/Source/rtc_sleep_port.c:207-212` | `cpu_frequency_conf()` | 当前复用 `SystemInit()`、`SystemCoreClockUpdate()`、`InitDelay()` 做 Stop 后时钟恢复。 |
| `103 + 309/Project/Source/System_Init.c:33-48` | `Init_IWDG()` | RTC 开启时 IWDG 配置为 prescaler 256、reload 0x0FFF，标称约 26.2s，最坏按 60kHz LSI 约 17.5s。 |

### 3.4 外设、通信和 BMS 业务

| 模块 | 位置 | 当前状态 |
|---|---|---|
| TIM3 主时基 | `103 + 309/Project/Source/System_Init.c:100-127`、`:258-299` | 10ms 主任务 tick，Stop 前关闭，醒后 `InitTimer()`。 |
| SysTick 延时 | `103 + 309/Project/Source/System_Init.c:132-172` | 只用于阻塞延时，不是主 tick；醒后需 `InitDelay()`。 |
| ADC/TIM2/DMA | `103 + 309/Project/Source/ADC.c:149-285`、`:463-517` | Stop 前 `ADC_StopForLowPower()`，醒后 `InitADC()`；首批采样需稳定窗口测试。 |
| CAN | `103 + 309/Project/Source/Can_HDX.c:865-933` | 已有 `Can_IsBusy()`、`Can_PrepareSleep()`、`Can_RtcWakeService()`，但低功耗准入未统一使用 `Can_IsBusy()`。 |
| Modbus/RS485 | `103 + 309/Project/Source/Sci_Upper.c:1577-1691`、`:2243-2256` | 已有 `Sci_IsAnyPortBusy()`，但尚未接入统一 `LP_BLOCK_COMM`。 |
| Flash/日志/参数 | `103 + 309/Project/Source/Flash.c:259-342`、`:456-565`、`:760-940`，`LogRecord.c:97-210` | 多条同步擦写/保存路径，缺少统一 `StorageFlash_IsBusy()`。 |
| SOC | `103 + 309/Project/Source/SocEnhance.c:1678-1764` | 睡前快照、RTC 休眠秒数用于静置/OCV 补偿；应保留 `LP_GetLastSleepSeconds()`。 |
| AFE/MOS/保护 | `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c:11-108`、`SH367309_Func.c:228-305`、`System_Monitor.c:72-86` | 睡前 AFE BSTATUS 阻塞，醒后同步 MOS/故障；不能唤醒后无条件完整 `InitAFE1()`。 |
| LED/按键 | `103 + 309/Project/Source/LedBar.c:817-990`、`:1018-1115` | 已有睡前保存 SOC、blank 和 GPIO 处理；缺少 `LP_BLOCK_LED_ACTIVE`。 |
| 过放深睡 | `103 + 309/Project/Source/rtc_sleep.c:154-179` | 低压 deep 优先级方向正确；当前 `DEEP_MODE` 是复位式 Stop，不是硬件 Standby。 |

## 4. 当前项目和推荐架构差异

| 项目 | 当前状态 | 推荐架构 | 差异结论 |
|---|---|---|---|
| 总体路径 | 已有 `HICCUP_MODE` Stop + RTC 周期唤醒 | `LP_STATE_*` 显式状态机 | 不需要推倒重写；应在现有路径外包一层 `app_lowpower`。 |
| 状态表达 | `mode/readyToSleep/blockReason` | `LP_STATE_RUN/IDLE_CHECK/PREPARE_SLEEP/STOP_SLEEP/WAKEUP_RESTORE/DEEP_STANDBY/ERROR` | 当前能运行但诊断不清晰。 |
| 阻塞原因 | 单值枚举，只覆盖电流、MCU_WAKE、老化、外部通信计数、AFE | `uint32_t` 位图，覆盖通信、Flash、升级、故障、LED、IWDG | 当前最大缺口。 |
| RTC | F1 Alarm + EXTI17 已完整 | `bsp_rtc` 屏蔽 F1/F0 差异 | 当前路径正确，后续应抽象并补全返回值检查。 |
| 时钟恢复 | `cpu_frequency_conf()` 复用 `SystemInit()` | `bsp_clock` 专用 Stop 后恢复 | 当前顺序正确，但长期可移植性和错误处理不足。 |
| IWDG | Stop 前后喂狗，当前 1s 周期安全 | 统一预算计算和 `LP_BLOCK_IWDG_UNSAFE` | 当前安全依赖 1 秒周期，不应依赖工程师记忆。 |
| 通信 | 有 `Can_IsBusy()`、`Sci_IsAnyPortBusy()`，未统一用 | 通信活跃禁止 Stop | 当前仍可能在半包、待 ACK、升级窗口误睡。 |
| Flash | 写入路径清晰，无 busy API | `StorageFlash_IsBusyOrPending()` 或 busy 计数 | 第一版必须补阻塞，不改 Flash 布局。 |
| AFE/MOS | SH367309 专用判断存在 | AFE busy/Fault/MOS 同步纳入低功耗合约 | 当前方向正确，需变成框架阻塞位。 |
| Standby/Deep | `DEEP_MODE` 是 reset-style Stop | Standby/SHIP 单独设计 | 现在不建议改成真正 Standby。 |
| 诊断 | 多数状态需 Keil Watch | 上位机可读状态/阻塞位/唤醒源 | 后续增强项。 |

## 5. 风险清单

### 5.1 P0 风险

| ID | 风险 | 依据 | 控制要求 |
|---|---|---|---|
| P0-01 | RTC/EXTI pending 未清导致 Stop 进不去或刚进即醒 | RM0008；`RTC.c:281-317`、`conf.c:374-382` | 固定 RTC/EXTI/NVIC pending 清除顺序，禁止分散调用 Stop。 |
| P0-02 | Stop 唤醒后时钟未恢复导致 CAN/USART/TIM/ADC 时序错误 | RM0008/AN2821；`conf.c:374-421`、`rtc_sleep_port.c:207-212` | `LP_AfterWakeup()` 第一动作必须恢复系统时钟，再恢复外设。 |
| P0-03 | RTC 周期超过 IWDG 安全窗口导致误复位 | RM0008；`System_Init.c:33-48`、`RTC.c:366-399` | 新增 `LP_BLOCK_IWDG_UNSAFE`，第一版非复位 Stop 周期建议不超过 10s。 |
| P0-04 | 通信半包、待 ACK、升级窗口中入睡 | RM0008；`Can_HDX.c:865-893`、`Sci_Upper.c:1678-1690`、`Runtime.c:23-29` | `Can_IsBusy()`、`Sci_IsAnyPortBusy()`、升级 pending 任一为真禁止 Stop。 |
| P0-05 | Flash 擦写/参数/日志/SOC 保存期间入睡 | AN2629/RM0008；`Flash.c:259-342`、`:456-565`、`LogRecord.c:97-210` | 新增 Flash busy/pending 阻塞；保存未完成不得进入可恢复 sleep。 |
| P0-06 | AFE/MOS 状态不同步导致保护或 MOS 输出错误 | AN2629；`rtc_sleep_afe_sh367309.c:11-108`、`System_Monitor.c:72-86` | 睡前 AFE 明确可睡；醒后重新读 AFE BSTATUS/MOS/Fault，并以 AFE 实际状态覆盖 MCU 缓存。 |

### 5.2 P1 风险

| ID | 风险 | 控制要求 |
|---|---|---|
| P1-01 | RTC/LSE/LSI 初始化失败或 safe wait 返回值未处理 | RTC 初始化失败必须返回 `LP_BLOCK_RTC_UNREADY` 或进入 `LP_STATE_ERROR`。 |
| P1-02 | Backup Domain 复用被 `BKP_DeInit()` 清除 | 建立 BKP 分配表；禁止无条件清备份域。 |
| P1-03 | SOC 休眠时间补偿丢失或 LSI 时间不可信 | 提供 `LP_GetLastSleepSeconds()` 和 RTC clock source/精度诊断。 |
| P1-04 | 新框架破坏当前外设恢复顺序 | 固化 `BspClock_RestoreAfterStop -> RTC/IO/ADC/SCI/CAN/TIM3/AFE` 顺序。 |
| P1-05 | 复位式睡眠和非复位式 Stop 语义混用 | 第一版只把 `HICCUP_MODE` 作为 Stop 主路径；`DEEP_MODE` 单独保留。 |
| P1-06 | LED/按键显示窗口被入睡抢占 | 新增 `LP_BLOCK_LED_ACTIVE`，显示窗口内延迟 Stop。 |

### 5.3 P2 风险

| ID | 风险 | 后续方向 |
|---|---|---|
| P2-01 | `blockReason` 是单值枚举，现场诊断不足 | 内部改位图，保留旧字段作为兼容摘要。 |
| P2-02 | HSE/LSE 实际硬件频率和起振裕量未纳入诊断 | 启动记录 HSE/LSE/LSI 状态，验证 Keil `CLOCK(12000000)` 与源码 `HSE_VALUE=8MHz` 差异。 |
| P2-03 | F0/F1 可移植接口误用 F1 RTC 模型 | `bsp_rtc` 加编译期能力宏，F0 Wakeup Timer/Alarm 分支分开。 |
| P2-04 | Debug 与量产 IWDG/低功耗行为差异未说明 | 测试记录必须标注 Debug/Release、是否接调试器、是否冻结 IWDG。 |

## 6. 最小优化方案

第一版目标：稳定 Stop + RTC 周期唤醒，不追求最低电流，不做 CAN/USART Stop 唤醒，不改变现有协议和业务算法。

### 6.1 最小架构

新增薄封装模块，优先复用现有实现：

- `app_lowpower.c/.h`：状态机、阻塞位图、IWDG 安全、业务准入。
- `bsp_rtc.c/.h`：包裹当前 `RTC.c` 的 F1 Alarm + EXTI17 能力，并预留 F0 分支。
- `bsp_power.c/.h`：统一 Stop/Standby 入口、PWR/EXTI pending 合约。
- `bsp_clock.c/.h`：Stop 后系统时钟恢复，短期可由 `cpu_frequency_conf()` 调用。

建议接口保持用户给出的形态：

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

### 6.2 状态机

第一版建议把现有 `HICCUP_MODE` 映射为：

```text
LP_STATE_RUN
  -> LP_STATE_IDLE_CHECK
  -> LP_STATE_PREPARE_SLEEP
  -> LP_STATE_STOP_SLEEP
  -> LP_STATE_WAKEUP_RESTORE
  -> LP_STATE_RUN 或继续下一轮 STOP
```

`LP_STATE_DEEP_STANDBY` 暂时只作为设计名称，不把当前 `DEEP_MODE` 改成硬件 Standby。当前 `DEEP_MODE` 仍按 `SleepDeal_Continue()` 的复位式 Stop 策略处理。

### 6.3 阻塞原因位图

内部使用 `uint32_t`，至少落地：

- `LP_BLOCK_CHARGE`
- `LP_BLOCK_DISCHARGE`
- `LP_BLOCK_COMM`
- `LP_BLOCK_KEY`
- `LP_BLOCK_AFE_BUSY`
- `LP_BLOCK_FLASH_BUSY`
- `LP_BLOCK_UPGRADE`
- `LP_BLOCK_FAULT`
- `LP_BLOCK_LED_ACTIVE`
- `LP_BLOCK_IWDG_UNSAFE`

旧 `g_stLowPowerRtcStatus.blockReason` 可保留，用于兼容当前诊断或上位机摘要。实际入睡判断必须用完整位图。

### 6.4 第一版准入规则

1. 低压 deep request 优先判断，但 `Flash busy`、`Upgrade pending` 仍可短暂阻塞以保证数据/流程完整。
2. 普通 Stop 必须同时满足：无充放电、无通信 busy、无 Flash busy、无升级 pending、无 AFE busy、无故障处理、无 LED 显示窗口、IWDG 周期安全。
3. 进入 `LP_STATE_PREPARE_SLEEP` 后二次采样位图，防止刚收到 Modbus/CAN 帧或刚置位 Flash 保存请求时误睡。
4. `LP_BeforeSleep()` 调用顺序固定为：二次准入、保存核心状态、LED 收口、ADC 关闭、通信收口、GPIO/唤醒源配置、RTC Alarm、清 pending、喂 IWDG。
5. `LP_AfterWakeup()` 调用顺序固定为：恢复系统时钟、`InitDelay()`、RTC 运行中断、IO、ADC、USART、CAN、TIM3、AFE IIC、AFE/MOS/保护同步、SOC 休眠补偿。

## 7. 后续增强方案

| 增强项 | 说明 | 前置条件 |
|---|---|---|
| 上位机可读低功耗诊断 | 暴露状态机、阻塞位图、最近唤醒源、最近休眠秒数、RTC 时钟源、IWDG unsafe 次数 | 最小框架稳定后，不破坏现有寄存器映射。 |
| F0 移植 | `bsp_rtc` 增加 F0 Wakeup Timer + EXTI20、Alarm + EXTI17 分支 | 完成 F1 行为冻结和测试。 |
| Standby/SHIP | 为过放、运输、仓储单独设计 BootFlag、BKP/Flash 快照、唤醒恢复 | 当前 reset-style deep 语义梳理完成。 |
| RTC/LSE/LSI 校准 | 用实测 LSI/LSE 误差优化 SOC 静置时间和 IWDG 预算 | 低功耗主链路稳定后。 |
| 通信唤醒 | 单独评估 CAN/USART Stop 唤醒、首帧丢失、波特率恢复、收发器供电 | 第一版不做。 |
| 最低电流优化 | 再审 GPIO、模拟输入、收发器、AFE SHIP、调试域 | 稳定睡眠/唤醒、协议和保护通过长稳测试后。 |

## 8. 测试矩阵摘要

P0 必测组合：

| 组合 | 目的 | 通过标准 |
|---|---|---|
| 空闲 Stop + RTC + IWDG | 验证主链路 | 至少 10 分钟无误复位，RTC 周期唤醒并可继续入睡。 |
| Stop 唤醒 + 时钟恢复 + Modbus | 验证 HSE/SysTick/TIM3/USART 恢复 | `COM4/19200/slave=1` 连续读 `0xD000` 成功。 |
| Stop 唤醒 + CAN 服务 | 验证 `Can_RtcWakeService()` | CAN 周期报文继续，`can-diag` 无持续 timeout/busoff。 |
| 通信活跃禁止休眠 | 防止协议半包 | 连续 Modbus/CAN 访问期间不进 Stop。 |
| Flash 保存禁止休眠 | 防止数据损坏 | 写入请求期间不进 Stop，重启后参数/日志/SOC 数据一致。 |
| AFE/MOS/保护同步 | 防止保护状态丢失 | AFE 异常阻塞或退出睡眠，MOS/故障状态正确同步。 |
| 过放深休眠 + 充电唤醒 | BMS 安全底线 | 低压触发 deep 路径，接充电后恢复通信和保护采样。 |

测试必须遵守仓库烧录安全规则：

- App 地址固定 `0x08004800`。
- IAP/Bootloader 地址固定 `0x08000000`。
- 烧录优先使用 `.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash`。
- 禁止裸写 `FD_Debug.bin` 或 `FD_Release.bin` 到 `0x08000000`。

## 9. 建议修改文件清单

第三阶段经确认后建议修改或新增：

### 9.1 新增文件

- `103 + 309/Project/Source/app_lowpower.c`
- `103 + 309/Project/Source/app_lowpower.h`
- `103 + 309/Project/Source/bsp_rtc.c`
- `103 + 309/Project/Source/bsp_rtc.h`
- `103 + 309/Project/Source/bsp_power.c`
- `103 + 309/Project/Source/bsp_power.h`
- `103 + 309/Project/Source/bsp_clock.c`
- `103 + 309/Project/Source/bsp_clock.h`

### 9.2 需要小步修改的现有文件

| 文件 | 建议修改 |
|---|---|
| `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx` | 加入新增 `.c/.h` 到 Keil 工程。 |
| `103 + 309/Project/Source/Runtime.c` 或 `rtc_sleep.c` | 短期让 `App_LowPowerProcess()` 进入 `LP_Task()`，或让 `LP_Task()` 包裹现有 `rtc_sleep()`。 |
| `103 + 309/Project/Source/rtc_sleep.h/.c` | 保留现有 `HICCUP_MODE` 行为，逐步把 block reason 折叠为新位图。 |
| `103 + 309/Project/Source/RTC.c/.h` | 包装到 `bsp_rtc`；检查 safe wait 返回值；恢复 IWDG 安全约束或移交给 `app_lowpower`。 |
| `103 + 309/Project/Source/conf/conf.c` | 保持 `Sys_StopMode()` 单一 Stop 入口，逐步由 `bsp_power/bsp_clock` 包装。 |
| `103 + 309/Project/Source/Can_HDX.c/.h` | 暴露 CAN busy/quiet window；避免通信 busy 时调用清队列的 `Can_PrepareSleep()`。 |
| `103 + 309/Project/Source/Sci_Upper.c/.h` | 接入 `Sci_IsAnyPortBusy()` 和串口静默窗口。 |
| `103 + 309/Project/Source/Flash.c/.h` | 增加 `StorageFlash_IsBusyOrPending()` 或 busy 计数。 |
| `103 + 309/Project/Source/LedBar.c/.h` | 增加 `LedBar_IsActiveForSleepBlock()`。 |
| `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c` | 保持专用 AFE 判断，接入 `LP_BLOCK_AFE_BUSY/FAULT`。 |

## 10. 不建议现在修改的内容

- 不把当前 F1 工程改成 F0 Wakeup Timer。
- 不做 CAN/USART Stop 唤醒；通信活跃只作为禁止睡眠原因。
- 不把当前 `DEEP_MODE` 改成真正 STM32 Standby。
- 不改 CAN/Modbus/RS485 协议、帧 ID、寄存器映射。
- 不改 SOC 算法本体、OCV 表和 Flash 存储布局。
- 不改 AFE 保护阈值、MOS 控制策略，不在 Stop 唤醒后无条件完整 `InitAFE1()`。
- 不切换到 72MHz PLL，不改 CAN/USART 时钟参数，除非先完成硬件 HSE 频率确认和全外设时序复核。
- 不追求最低电流，不做大规模无关重构。

## 11. 最终结论

当前项目并不是没有低功耗基础：`rtc_sleep.c`、`rtc_sleep_port.c`、`RTC.c`、`conf.c` 已经形成 STM32F103 的 Stop + RTC Alarm 周期唤醒闭环，并且已有 Stop 后时钟和外设恢复。第一版优化不应推翻该路径。

最小可行方向是：在现有 `HICCUP_MODE` 主路径外收敛出 `app_lowpower + bsp_rtc + bsp_power + bsp_clock`，补齐阻塞原因位图、IWDG 安全窗口、通信 busy、Flash busy、LED active 和升级 pending。这样能优先满足稳定睡眠、稳定唤醒、通信不乱、保护不丢、IWDG 不误复位，同时为后续 STM32F0/F1 项目复用保留接口。

