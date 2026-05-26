# IWDG 低功耗策略设计

更新时间：2026-05-26  
阶段：第一阶段输出的第二阶段设计建议，不修改源码  
适用范围：当前 STM32F103 BMS App，以及后续可移植到 STM32F0/F1 的 RTC Stop 低功耗框架。

## 设计目标

先保证稳定睡眠、稳定唤醒、通信不乱、保护不丢、IWDG 不误复位；不以最低电流为第一目标。

本策略不改变现有 Modbus/CAN 协议，不改变 SOC、保护、AFE、Flash、LED 业务语义。第三阶段实现时，只应把 IWDG 安全判断收敛到低功耗框架入口，避免在业务模块里继续散落判断。

## 官方约束转成工程规则

1. IWDG 一旦启动，Stop/Standby 中不能假设它停止，除 Reset 外不能停止。
2. IWDG 使用独立 LSI，必须按 LSI 最快值计算最短超时。
3. Stop 中 SysTick/TIM3 主时基不会继续推进，不能靠主循环喂狗。
4. RTC 周期唤醒必须早于 IWDG 最短超时，并预留醒后恢复和业务检查时间。
5. 若需要超过 IWDG 窗口的长睡眠，应走复位式 Standby/Stop 策略，并明确“不启动软件 IWDG 后再睡”的启动路径；不能在已启动 IWDG 的 Stop 中赌它不复位。

## 当前项目基线

当前源码基线如下：

- `Project_Config.h:80-86`：`PROJECT_CFG_WDOG_ENABLE=1`，`PROJECT_CFG_RTC_ENABLE=1`。
- `conf.h:49-55`：派生 `wdog_enable` 和 `__FUNC_RTC__`。
- `System_Init.c:33-48`：当前实际 IWDG 为 `IWDG_Prescaler_256` + `IWDG_SetReload(0x0FFF)`。
- `Can_HDX.c:23`：当前 RTC 周期 `FEIDAO_CAN_RTC_PERIOD_SECONDS=1`。
- `RTC.c:366-399`：当前 `RTC_GetWakeupPeriodSeconds()` 不再裁剪 IWDG 安全窗口。
- `rtc_sleep_port.c:118-123`：Stop 前后喂狗。
- `Runtime.c:32-42`：运行态后台任务末尾喂狗。
- `SleepDeal.c:83-115`、`SleepDeal.c:186-230`：NORMAL/DEEP 使用复位式 Stop，软件 IWDG 在 `IsSleepStartUp()` 后才会启动。

当前结论：现状 1 秒 RTC Stop 周期安全；设计上必须防止后续把周期改大后绕过 IWDG 窗口。

## IWDG 超时计算策略

建议低功耗框架内部统一维护以下概念，不要继续把公式写在 `RTC.c` 注释里：

```c
T_iwdg_nominal_ms = (reload + 1) * prescaler * 1000 / LSI_TYP_HZ;
T_iwdg_min_ms     = (reload + 1) * prescaler * 1000 / LSI_MAX_HZ;
T_stop_budget_ms  = T_iwdg_min_ms - T_restore_budget_ms - T_service_budget_ms - T_margin_ms;
```

针对当前配置：

| 参数 | 当前值 | 说明 |
|---|---:|---|
| `reload` | 4095 | `System_Init.c:42` |
| `prescaler` | 256 | `System_Init.c:41` |
| `LSI_TYP_HZ` | 40000 | 当前源码注释按 40 kHz 计算 |
| `LSI_MAX_HZ` | 60000 | 未实测时按保守最快值估算 |
| 标称超时 | 约 26.2 s | 40 kHz |
| 最短估算超时 | 约 17.5 s | 60 kHz |

建议第一版安全规则：

- 若 IWDG 已启用且 Stop 后不复位：`RTC_WAKEUP_PERIOD_SEC <= 10 s`。
- 当前 CAN RTC 周期 1 秒保持不变。
- 后续若需要可配周期，默认上限取 `min(10, T_iwdg_min*60%)`，当前即约 10 秒。
- 若实测 LSI 并建立校准值，可把上限放宽到 `T_iwdg_min - 5 s`，但必须写入文档和测试记录。

## RTC 周期与 IWDG 的安全关系

必须满足：

```text
T_rtc_period
+ T_clock_restore
+ T_after_wakeup_feed_latency
+ T_rtc_service
+ T_jitter_margin
< T_iwdg_min
```

当前项目映射：

- `T_rtc_period`：来自 `RTC_GetWakeupPeriodSeconds()`，当前通过 `Can_GetIdleRtcPeriodSeconds()` 返回 1 秒。
- `T_clock_restore`：`Sys_StopMode()` 返回后调用 `cpu_frequency_conf()`，见 `conf.c:374-385`；`cpu_frequency_conf()` 调用 `SystemInit()`、`SystemCoreClockUpdate()`、`InitDelay()`，见 `rtc_sleep_port.c:207-212`。
- `T_after_wakeup_feed_latency`：`RtcSleep_PortEnterStop()` 在 `Sys_StopMode()` 返回后立刻喂狗，见 `rtc_sleep_port.c:118-123`。
- `T_rtc_service`：无异常 RTC 醒来后可能执行 `Can_RtcWakeService()`，见 `rtc_sleep.c:323-327`；CAN 服务等待循环最多约 1.5 秒，见 `Can_HDX.c:23-24`、`Can_HDX.c:906-933`。
- `T_jitter_margin`：建议第一版固定 5 秒，覆盖 LSI 偏差、唤醒延迟、恢复外设、日志/显示偶发延迟。

当前 1 秒周期满足安全关系。后续不允许把 RTC 周期直接改成 30 秒、60 秒或分钟级，除非先改为复位式深睡策略或重新计算 IWDG 窗口。

## 建议接口落点

第三阶段新增 `app_lowpower.c/.h` 时，IWDG 策略应放在低功耗框架，而不是继续放在 `RTC.c` 或 CAN 模块：

```c
uint8_t LP_CanSleep(void);
uint32_t LP_GetBlockReason(void);
void LP_SetWakeupPeriod(uint32_t seconds);
void LP_EnterStop(uint32_t seconds);
```

建议新增内部接口：

```c
uint32_t LP_IwdgGetMinTimeoutMs(void);
uint32_t LP_IwdgGetSafeStopBudgetMs(void);
uint8_t LP_IwdgIsWakePeriodSafe(uint32_t seconds);
```

第一版可以先用编译期常量匹配当前配置：

```c
#define LP_IWDG_LSI_MAX_HZ             60000UL
#define LP_IWDG_RELOAD_RTC             0x0FFFUL
#define LP_IWDG_PRESCALER_RTC          256UL
#define LP_IWDG_WAKE_MARGIN_MS         5000UL
#define LP_IWDG_RTC_SERVICE_BUDGET_MS  2000UL
```

后续若要支持非 RTC 配置，也应显式区分 `__FUNC_RTC__` 下的 256/4095 和非 RTC 下的 64/800。

## 禁止休眠原因建议

当前用户建议的 `LP_BLOCK_IWDG_UNSAFE` 必须落地。触发条件建议：

1. `PROJECT_CFG_WDOG_ENABLE=1` 且目标 Stop 周期大于 IWDG 安全预算。
2. RTC 未初始化成功或 `RTC_GetWakeupPeriodSeconds()` 返回 0 后仍无法设置成安全值。
3. 目标模式是 HICCUP_MODE/Stop 续跑，但检测到硬件 IWDG option byte 可能已启用且启动路径不明确。
4. 醒后服务预算不可控，例如 CAN/Flash/AFE 仍处在忙等待或无超时流程中。

当前项目已有相关 block reason：

- `rtc_sleep.h:39-48`：已有 `LOW_POWER_RTC_BLOCK_CURRENT`、`LOW_POWER_RTC_BLOCK_MCU_WAKE`、`LOW_POWER_RTC_BLOCK_FACTORY_AGING`、`LOW_POWER_RTC_BLOCK_EXT_COMM`、`LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE`。

后续迁移到新框架时建议新增：

- `LP_BLOCK_IWDG_UNSAFE`
- `LP_BLOCK_RTC_UNREADY`
- `LP_BLOCK_FLASH_BUSY`
- `LP_BLOCK_COMM`

不要把 IWDG 不安全混进 `LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE` 或通信阻塞原因里，否则现场诊断会失真。

## 当前代码的最小优化建议

第三阶段经确认后，建议按以下最小改动顺序实现：

1. 新增 `app_lowpower` 的 IWDG 安全预算函数，先不改业务状态机。
2. 把 `RTC_GetWakeupPeriodSeconds()` 中被注释的 IWDG 安全裁剪迁移到 `LP_SetWakeupPeriod()` 或 `LP_EnterStop()`，不要继续让 `RTC.c` 依赖 CAN/IWDG 策略细节。
3. 当前 `FEIDAO_CAN_RTC_PERIOD_SECONDS=1` 保持不变。
4. `LP_CanSleep()` 增加 IWDG 安全判断；不安全时返回禁止休眠，并置 `LP_BLOCK_IWDG_UNSAFE`。
5. `LP_BeforeSleep()` 中明确喂狗一次，再配置 RTC Alarm，再清 pending，再进入 Stop。
6. `LP_AfterWakeup()` 中先恢复系统时钟，再尽早喂狗，再恢复 RTC/SysTick/TIM/ADC/UART/CAN/LED。
7. 保留当前 `Runtime_RunBackgroundTasks()` 末尾喂狗，后续再评估是否改成“关键任务全部完成后喂狗”的更强约束。

## 不建议现在修改的内容

第一阶段和第二阶段不建议修改：

- 不改 `Init_IWDG()` 的 prescaler/reload。
- 不把 RTC 周期从 1 秒直接改长。
- 不改 CAN/Modbus 协议和收发时序。
- 不做 CAN/USART Stop 唤醒。
- 不把 NORMAL/DEEP 复位式睡眠改成真正 Standby。
- 不移除已有底层喂狗点，避免引入新的误复位。
- 不做大规模 Runtime 任务重排。

## 测试要求

第三阶段实现 IWDG 安全判断后，至少做以下验证：

| 用例 | 操作 | 预期 |
|---|---|---|
| 当前默认 | `PROJECT_CFG_WDOG_ENABLE=1`、`PROJECT_CFG_RTC_ENABLE=1`、RTC 周期 1 秒 | 可进入 HICCUP Stop，周期醒来，不 IWDG 复位 |
| 周期超限防护 | 人为设置目标 RTC 周期大于安全预算 | `LP_CanSleep()` 禁止 Stop，`LP_BLOCK_IWDG_UNSAFE` 可读 |
| RTC 不唤醒故障 | 临时屏蔽 RTC Alarm/EXTI17，仅台架验证 | 已启动 IWDG 后应在窗口内复位，复位原因可观察 |
| 醒后服务 | CAN RTC 服务无 ACK/Bus busy | 服务循环内喂狗，超时退出，不拖垮 IWDG |
| 复位式睡眠 | NORMAL/DEEP 通过 `SleepDeal_Continue()` | 复位后在 `Init_IWDG()` 前进入 Stop；若未配置硬件 IWDG，不受软件 IWDG 限制 |
| Debug 断点 | Debug 目标下断点停留超过窗口 | 当前会有 IWDG 复位风险，后续若启用 `EnableLowPowerDebug()` 需单独记录 |

## 结论

当前项目 IWDG 与 RTC Stop 的实际运行安全性依赖两个事实：IWDG 长窗口约 26.2 秒，RTC 周期当前只有 1 秒。现状可以继续作为第一版低功耗分析基线。

后续可复用框架必须把 “RTC 周期 < IWDG 最短超时并留恢复预算” 做成统一入口规则。不能依赖工程师记得不要改 `FEIDAO_CAN_RTC_PERIOD_SECONDS`，也不能把被注释的裁剪逻辑长期留在 `RTC.c`。
