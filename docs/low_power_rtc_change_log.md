# RTC 低功耗框架变更记录

本文件记录 RTC 低功耗框架相关文档、设计和后续代码变更。  
当前阶段只读分析，不修改源码，不提交。

## 2026-05-26 DocsAgent 汇总

### 变更类型

文档新增。

### 新增文件

- `docs/low_power_rtc_final_report.md`
- `docs/low_power_rtc_migration_plan.md`
- `docs/low_power_rtc_change_log.md`

### 未做事项

- 未修改任何源码。
- 未修改 Keil 工程。
- 未编译固件。
- 未烧录。
- 未创建 git 提交。

### 输入文档

汇总读取了以下 subagent 输出：

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

### 本次结论

1. 当前项目是 `STM32F103C8` 标准外设库工程，低功耗主路径应按 F1 `RTC Alarm + EXTI17 + Stop` 分析。
2. 当前项目已有可用低功耗雏形：`rtc_sleep.c` 策略层、`rtc_sleep_port.c` 适配层、`RTC.c` RTC/EXTI17、`conf.c` Stop 和恢复。
3. 第一版不应推翻现有 `HICCUP_MODE`；应把它收敛为 `Stop + RTC 周期唤醒` 主路径。
4. 当前 `NORMAL_MODE/DEEP_MODE` 是复位式睡眠策略，不是硬件 Standby；第一版不改其语义。
5. 当前最大缺口是阻塞原因不完整：缺少通信 busy、Flash busy、升级 pending、Fault、LED active、IWDG unsafe。
6. 当前 `RTC_GetWakeupPeriodSeconds()` 的 IWDG 安全裁剪被注释；后续最小实现必须恢复为框架级硬约束。
7. Stop 唤醒后时钟恢复顺序当前基本正确，但长期应从 `SystemInit()` 复用改为 `bsp_clock` 专用接口。
8. 第一版不做 CAN/USART Stop 唤醒，通信活跃时禁止休眠。

### 本次风险登记

P0：

- RTC/EXTI pending 未清导致 Stop 进不去或刚进即醒。
- Stop 唤醒后系统时钟未恢复导致 CAN/USART/TIM/ADC 时序错误。
- RTC 周期超过 IWDG 安全窗口导致 Stop 中误复位。
- 通信半包、待 ACK 或升级窗口中入睡导致协议乱序。
- Flash 擦写/参数/日志/SOC 保存期间入睡导致数据风险。
- AFE/MOS 状态不同步导致保护或输出错误。

P1：

- RTC/LSE/LSI 初始化失败处理不完整。
- Backup Domain 复用和 `BKP_DeInit()` 冲突。
- SOC 休眠时间补偿不可信。
- 外设恢复顺序被新框架破坏。
- 复位式睡眠与非复位式 Stop 语义混用。
- LED/按键显示窗口被入睡抢占。

P2：

- 阻塞原因单值枚举不利于诊断。
- HSE/LSE 实际硬件状态未纳入诊断。
- F0/F1 可移植接口容易误用 F1 RTC 模型。
- Debug 与量产 IWDG/低功耗行为差异需要记录。

## 2026-05-26 第二阶段设计收口

### 变更类型

文档新增。

### 新增文件

- `docs/design/low_power_minimal_architecture.md`
- `docs/design/low_power_api_state_machine.md`
- `docs/design/low_power_integration_scope.md`
- `docs/design/low_power_phase2_design_summary.md`

### 变更摘要

1. 明确第三阶段最小实现继续沿用 `Stop + RTC Alarm + EXTI17`，不改为 Standby。
2. 明确新增 `app_lowpower`、`bsp_rtc`、`bsp_power`、`bsp_clock` 四组模块。
3. 明确 `LP_*` 接口、低功耗状态机和禁止休眠原因位图。
4. 明确第三阶段不修改 CAN/Modbus 协议、SOC 核心算法、AFE/MOS 策略、Flash 布局、IAP/App 地址和烧录脚本。

### 风险影响

- P0 风险未关闭，第三阶段必须优先处理 RTC pending、时钟恢复、IWDG 安全窗口、通信 busy、Flash busy、AFE/MOS 同步。
- 第二阶段没有源码改动，不影响 CAN/Modbus/SOC/AFE/Flash/LED/IWDG/RTC 的现有运行行为。

### 验证结果

- 编译：未执行，第二阶段仅文档设计。
- 烧录：未执行。
- 上板：未执行。

### 文档同步

- 已更新：`docs/design/low_power_phase2_design_summary.md`
- 已更新：`docs/low_power_rtc_change_log.md`

## 2026-05-26 第三阶段最小实现

### 变更类型

- 源码
- 文档
- 构建

### 修改文件

- `103 + 309/Project/Source/app_lowpower.c`
- `103 + 309/Project/Source/app_lowpower.h`
- `103 + 309/Project/Source/bsp_rtc.c`
- `103 + 309/Project/Source/bsp_rtc.h`
- `103 + 309/Project/Source/bsp_power.c`
- `103 + 309/Project/Source/bsp_power.h`
- `103 + 309/Project/Source/bsp_clock.c`
- `103 + 309/Project/Source/bsp_clock.h`
- `103 + 309/Project/Source/AppInit.c`
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep.h`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/RTC.h`
- `103 + 309/Project/Source/Flash.c`
- `103 + 309/Project/Source/Flash.h`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/LedBar.h`
- `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`
- `docs/implementation/low_power_phase3_minimal_implementation.md`
- `docs/low_power_rtc_change_log.md`

### 变更摘要

1. 新增 `app_lowpower`、`bsp_rtc`、`bsp_power`、`bsp_clock` 四组最小框架模块。
2. `AppInit_Boot()` 在现有 `Init_RTC()` 后初始化低功耗框架。
3. `Runtime_RunIoAndPowerTasks()` 使用 `LP_Task()` 承接原低功耗周期任务。
4. 原 `rtc_sleep()` 保留，仅通过 `LP_CanSleep()` 增加通信、Flash、升级、LED、故障、IWDG 等阻塞约束。
5. RTC 唤醒周期增加框架覆盖接口，并在 IWDG 开启时限制到 10 秒安全窗口。
6. Flash 增加同步写入 busy 标志，LED 增加低功耗活跃状态查询。
7. Keil `FD_Release` 和 `FD_Debug` 两个 target 均加入新增 C 文件。

### 风险影响

- P0：IWDG 误复位风险降低，RTC 周期被限制到 10 秒安全窗口。
- P0：通信半包入睡风险降低，`Sci_IsAnyPortBusy()`、`Can_IsBusy()`、`Can_IsBusActive()` 会阻塞 Stop。
- P0：Flash 写入窗口入睡风险降低，新增 `StorageFlash_IsBusy()` 与升级 pending 阻塞。
- P1：LED 显示窗口入睡风险降低，新增 `LedBar_IsActiveForLowPower()`。
- 剩余风险：仍需上板验证 Stop 电流、RTC 唤醒、外设恢复和 CAN/Modbus 通信连续性。

### 验证结果

- 编译：通过。
- 工程：`103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`
- Target：`FD_Release`
- 编译器：ARMCC `V5.06 update 7 build 960`
- 结果：`0 Error(s), 0 Warning(s)`
- 大小：`Code=51076 RO-data=2964 RW-data=816 ZI-data=6040`
- BIN：`103 + 309/Project/Users/Objects/FD_Release.bin`
- 烧录：未执行。
- Modbus：未上板验证。
- CAN：未上板验证。
- RTC Stop：未上板验证。
- IWDG：未上板验证。
- Flash：仅编译验证，未做掉电/擦写实测。
- AFE/MOS：未上板验证。
- SOC：未上板验证。
- LED/按键：未上板验证。

### 文档同步

- 已新增：`docs/implementation/low_power_phase3_minimal_implementation.md`
- 已更新：`docs/low_power_rtc_change_log.md`

## 后续变更记录模板

后续每次代码或文档变更按此模板追加：

```markdown
## YYYY-MM-DD 变更标题

### 变更类型

- 文档 / 源码 / 测试 / 构建 / 烧录 / 上板验证

### 修改文件

- `path/to/file`

### 变更摘要

1. 
2. 
3. 

### 风险影响

- P0/P1/P2 风险是否变化。
- 是否影响 CAN/Modbus/SOC/AFE/Flash/LED/IWDG/RTC。

### 验证结果

- 编译：
- 烧录：
- Modbus：
- CAN：
- RTC Stop：
- IWDG：
- Flash：
- AFE/MOS：
- SOC：
- LED/按键：

### 文档同步

- 已更新：
- 未完成：
```

## 后续提交要求

当进入第三阶段并产生较大代码变更时，必须：

1. 同步更新设计、迁移、风险或测试文档。
2. 编译最新固件。
3. 按仓库规则生成描述清楚的 git 提交。
4. 烧录测试必须使用安全脚本，App 地址 `0x08004800`，不得覆盖 `0x08000000` IAP。
