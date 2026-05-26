# RTC 低功耗框架迁移计划

日期：2026-05-26  
范围：当前 STM32F103 BMS 项目，后续兼容 STM32F0/F1  
当前阶段：第一阶段只读分析完成，第二阶段设计方案形成；第三阶段必须等确认后才能修改源码。  
约束：不破坏现有 Modbus/CAN 协议，不破坏 SOC、保护、AFE、Flash、LED、IWDG、RTC、任务调度功能；每一步代码修改后同步更新文档。

## 1. 迁移目标

设计并逐步实现一个可复用的 RTC 低功耗框架：

- 第一版只做 `Stop + RTC 周期唤醒`。
- 当前 STM32F103 使用 `RTC Alarm + EXTI17`。
- 后续 STM32F0 按芯片能力选择 `RTC Wakeup Timer + EXTI20` 或 `Alarm + EXTI17`。
- 通信活跃时禁止休眠，不做 CAN/USART Stop 唤醒。
- Flash 擦写或保存未完成时禁止休眠。
- RTC 周期必须小于 IWDG 最短超时，除非明确进入复位式深睡策略。
- 唤醒后先恢复系统时钟，再恢复外设。
- SOC 保留 RTC 休眠秒数，用于静置/OCV 补偿。
- AFE/MOS/保护状态唤醒后必须重新同步。

## 2. 阶段划分

### 阶段 1：只读分析，已完成

已完成内容：

- 官方资料调研：STM32F0/F1 RTC、Stop、Standby、Sleep、IWDG、SysTick、LSE/LSI、Wakeup/Alarm。
- 当前项目扫描：RTC、PWR、RCC、SysTick、TIM、IWDG、ADC、CAN、UART、Modbus、AFE、SOC、Flash、LED。
- 风险清单：P0/P1/P2。
- 测试矩阵和手工测试步骤。
- DocsAgent 汇总文档。

阶段 1 不修改源码、不编译、不提交。

### 阶段 2：设计冻结，当前建议

冻结以下设计点后再进入实现：

1. 第一版主路径固定为 `HICCUP_MODE = Stop + RTC 周期唤醒`。
2. `NORMAL_MODE/DEEP_MODE` 暂保留当前复位式睡眠语义，不改成硬件 Standby。
3. 第一版外部唤醒源只保留 RTC、充电、按键/MCU_WAKE；通信只作为禁止休眠原因。
4. 内部低功耗判断使用 `uint32_t` 阻塞位图，旧 `blockReason` 保留为兼容摘要。
5. IWDG 安全窗口由 `app_lowpower` 统一判断，不长期留在 `RTC.c` 注释中。
6. Stop 唤醒后恢复顺序固定为：系统时钟 -> Delay/SysTick -> RTC运行态 -> GPIO -> ADC -> USART -> CAN -> TIM3 -> AFE IIC -> AFE/MOS/保护同步 -> SOC补偿。

## 3. 最小实现路线

### 步骤 1：新增框架空壳，不改变行为

新增：

- `103 + 309/Project/Source/app_lowpower.c`
- `103 + 309/Project/Source/app_lowpower.h`
- `103 + 309/Project/Source/bsp_rtc.c`
- `103 + 309/Project/Source/bsp_rtc.h`
- `103 + 309/Project/Source/bsp_power.c`
- `103 + 309/Project/Source/bsp_power.h`
- `103 + 309/Project/Source/bsp_clock.c`
- `103 + 309/Project/Source/bsp_clock.h`

第一步要求：

- `LP_Init()` 可为空或只初始化内部状态。
- `LP_Task()` 先调用现有 `rtc_sleep()`，不改变行为。
- `BspClock_RestoreAfterStop()` 先包裹现有 `cpu_frequency_conf()` 逻辑。
- `BspRtc_*` 先包裹现有 `RTC_WKTimeConfig()`、`RTC_DisableStopWakeup()`、`RTC_RestoreRunInterrupts()`。
- `BspPower_EnterStop()` 先包裹现有 `Sys_StopMode()`。

验收：

- 编译通过。
- 默认运行行为与当前一致。
- 空闲 RTC Stop、Modbus 读取、CAN 诊断、IWDG 10 分钟不复位通过。

### 步骤 2：加入阻塞位图和兼容摘要

新增内部位图：

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

接入原则：

- `LP_GetBlockReason()` 返回完整位图。
- `g_stLowPowerRtcStatus.blockReason` 保留为旧字段，只做位图折叠摘要。
- 先实现只读诊断，不急着改变现有入睡决策。

验收：

- 当前旧低功耗逻辑仍然可用。
- 能在调试构建中观察完整位图。
- 旧 block reason 不破坏现有使用点。

### 步骤 3：接入 IWDG 安全窗口

实现：

- `LP_IwdgGetMinTimeoutMs()`
- `LP_IwdgGetSafeStopBudgetMs()`
- `LP_IwdgIsWakePeriodSafe(seconds)`

当前配置建议常量：

```c
#define LP_IWDG_LSI_MAX_HZ             60000UL
#define LP_IWDG_RELOAD_RTC             0x0FFFUL
#define LP_IWDG_PRESCALER_RTC          256UL
#define LP_IWDG_WAKE_MARGIN_MS         5000UL
#define LP_IWDG_RTC_SERVICE_BUDGET_MS  2000UL
```

第一版规则：

- `PROJECT_CFG_WDOG_ENABLE=1` 且非复位式 Stop 时，RTC 周期不超过 10 秒。
- 超限时置 `LP_BLOCK_IWDG_UNSAFE` 并禁止 Stop。
- 当前 `FEIDAO_CAN_RTC_PERIOD_SECONDS=1` 保持不变。

验收：

- 默认 1 秒周期不阻塞。
- 人为测试大周期时能阻塞并暴露 `LP_BLOCK_IWDG_UNSAFE`。

### 步骤 4：接入通信 busy 阻塞

RS485/Modbus：

- 使用 `Sci_IsAnyPortBusy()`。
- 继续保留 `RTC_ExtComCnt` 作为最近外部通信窗口。
- 后续可加入 `last_sci_activity_tick` 和静默窗口，初值建议 3 秒。

CAN：

- 使用 `Can_IsBusy()` 判断 TX 队列、邮箱、读块流和硬件邮箱。
- 不直接用粘性的 `Can_IsBusActive()` 作为唯一阻塞。
- 后续新增 CAN 最近活动时间，形成 quiet window。
- 确认 `Can_IsBusy()==0` 且静默窗口满足后，才允许调用 `Can_PrepareSleep()`。

验收：

- 连续 Modbus 轮询时不进入 Stop。
- CAN 队列未空时不进入 Stop。
- 通信停止并静默后可正常进入 Stop。

### 步骤 5：接入 Flash/升级阻塞

建议新增：

```c
uint8_t StorageFlash_IsBusyOrPending(void);
```

覆盖：

- SOC 快照保存。
- AFE 参数保存。
- RW 参数保存。
- 日志保存。
- 老化进度保存。
- 升级标志和 IAP pending。

短期如果无法一次性梳理所有 pending，可先在 Flash 写入临界区维护 busy 计数，再逐步接入 pending 标志。

验收：

- Flash 写入期间 `LP_BLOCK_FLASH_BUSY` 有效。
- 写参数、写 SOC、写日志后不会在 ACK 或保存未完成前入睡。
- 重启后数据一致。

### 步骤 6：接入 LED、AFE、Fault 阻塞

LED：

- 新增 `LedBar_IsActiveForSleepBlock()` 或等价封装。
- 按键 SOC 显示、老化剩余时间显示、充电图标刷新窗口内置 `LP_BLOCK_LED_ACTIVE`。

AFE：

- 复用 `RtcSleep_AfePortIsSleepBlocked()`。
- 后续扩展 AFE EEPROM/MTP 写状态。

Fault：

- 关键 AFE fault、MOS 状态异常、系统错误处理中置 `LP_BLOCK_FAULT`。
- 低压 deep request 与 fault block 分开记录，不把过放当普通 block。

验收：

- AFE BSTATUS 非空时不进入普通 Stop。
- LED 显示窗口不被立刻黑屏入睡。
- 过放 deep 不被普通 LED/通信永久阻塞。

### 步骤 7：收敛 Before/After 合约

`LP_BeforeSleep()` 固化：

1. 二次 `LP_CanSleep()`。
2. `LowPowerSleep_SaveCoreState()`。
3. LED sleep/blank。
4. `ADC_StopForLowPower()`。
5. 通信收口。
6. GPIO 低功耗态。
7. RTC Alarm 配置。
8. 清 RTC/EXTI/NVIC pending。
9. 喂 IWDG。

`LP_AfterWakeup()` 固化：

1. `BspClock_RestoreAfterStop()`。
2. `InitDelay()`。
3. `RTC_RestoreRunInterrupts()`。
4. `InitIO_rtc()`。
5. `ADC_StopForLowPower()` + `InitADC()`。
6. `USART_DeInit()` + `AppInit_InitSci()`。
7. `InitCan()`。
8. `InitTimer()`。
9. `initAFE1_IIC()`。
10. AFE/MOS/保护同步。
11. SOC 休眠补偿。

验收：

- 唤醒后 Modbus、CAN、ADC、TIM3、AFE 均恢复。
- 不补跑休眠期间全部 10ms/200ms 任务，休眠时间只通过 RTC 反馈给 SOC。

## 4. 编译与工程集成

第三阶段实现后必须：

1. 更新 Keil 工程 `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`，加入新增 `.c` 文件。
2. 使用项目既有构建方式生成 `FD_Release.bin`。
3. 不改变 App scatter：`103 + 309/Project/Users/Objects/FD_Release.sct`。
4. 不改变 App 地址 `0x08004800`。
5. 若修改用户上位机，才需要立即编译最新 exe；本 RTC 低功耗框架第一版不应涉及上位机 exe。

## 5. 文档同步要求

每一步代码改动后必须更新至少对应文档：

- 设计变更：`docs/low_power_rtc_final_report.md`
- 迁移进度：`docs/low_power_rtc_migration_plan.md`
- 风险状态：`docs/risk/low_power_risk_list.md`
- 测试结果：`docs/test/low_power_test_matrix.md` 或新增测试记录
- 变更记录：`docs/low_power_rtc_change_log.md`

若涉及以下规则，必须写入仓库脚本或文档，不能只留在对话中：

- 烧录地址。
- 测试模式。
- 量产隔离。
- 上位机启动方式。
- IWDG 与 RTC 周期安全关系。
- Stop 唤醒后的时钟恢复顺序。

## 6. 不迁移或暂缓项

第一版不做：

- CAN Stop 唤醒。
- USART Stop 唤醒。
- STM32 Standby/SHIP 真正深睡。
- RTC/LSE 精准校准。
- 低功耗电流极限优化。
- CAN/Modbus 协议变更。
- SOC 算法大改。
- AFE 参数和 MOS 策略大改。
- Flash 存储布局变更。
- Runtime 大规模任务重排。

## 7. 阶段验收标准

### 最小实现验收

- Release 编译通过。
- 默认量产配置 `PROJECT_CFG_BUILD_PROFILE=0`。
- `PROJECT_CFG_WDOG_ENABLE=1`、`PROJECT_CFG_RTC_ENABLE=1` 保持。
- 空闲可进入 Stop + RTC 周期唤醒。
- IWDG 不误复位。
- 连续 Modbus/CAN 活跃时不入睡。
- Flash busy 时不入睡。
- AFE/MOS/Fault 异常时不入普通 Stop。
- 唤醒后 Modbus/CAN/ADC/TIM3/LED/AFE 恢复。

### 上板回归验收

使用 `docs/test/low_power_test_matrix.md` 的 P0 必测组合：

- LP-T004：Stop 进入。
- LP-T005：RTC 周期唤醒。
- LP-T006：Stop 退出恢复。
- LP-T007：IWDG 安全。
- LP-T012：通信活跃禁止休眠。
- LP-T018：Flash busy 禁止休眠。
- LP-T020：充电唤醒。
- LP-T021：过放深休眠。
- LP-T024：长时间稳定性。

烧录必须使用：

```powershell
.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash
```

禁止把 App bin 写到 `0x08000000`。

## 8. 迁移结论

迁移策略应保持小步、可回退、可测试。当前最值得保留的是 `rtc_sleep_run_hiccup_cycle()`、`RTC_WKTimeConfig()`、`Sys_StopMode()`、`InitRunAfterStopWakeup()` 形成的主链路；当前最需要补齐的是阻塞位图、通信 busy、Flash busy、IWDG unsafe 和文档化的 before/after 合约。

