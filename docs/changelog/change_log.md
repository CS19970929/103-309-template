# 文档变更记录

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：当前主工程源码和 `docs/review/*`
最后更新时间：2026-06-02
未确认事项：`NEED_CONFIRM` 文档仍需用户确认是否保留；部分旧文档仍被 `tools/project_check.py` 固定引用。

## 2026-06-02 SystemDebug 模块健康总览增强

### 本次源码修改

- `SystemDebug.h`：新增 `DBG_MODULE_ID`、`DBG_MODULE_STATE_*` 和 `g_dbg.module`，用于按模块观察运行心跳、ready、busy、error 和 stale 状态。
- `SystemDebug.c`：新增 `SystemDebug_ModuleHeartbeat()`，记录每个模块的 `last_tick/max_gap_ticks/run_cnt`，并维护 `alive_mask/ready_mask/busy_mask/error_mask/stale_mask`。
- `Runtime.c`：在主循环任务执行后接入模块心跳，覆盖 `systime/aging/led/afe/soc/snapshot/sci/adc/low_power/can/flash/log/proid/watchdog/runtime` 等模块。
- `SystemDebug.c`：根据已有系统错误、CAN busoff、Flash busy、低功耗 ready、保护 fault 等状态刷新模块 `busy_mask` 和 `error_mask`。

### 安全约束

- 未修改业务流程、协议寄存器、CAN ID、CAN payload、IAP/App 地址和 SOC/AFE 参数。
- `PROJECT_CFG_DEBUG_MONITOR_ENABLE=0` 时 `SystemDebug_ModuleHeartbeat()` 为空实现。
- `stale_mask` 仅基于心跳 tick 判断，阈值为 200 个 10ms tick，约 2s。

### 本次验证

- Keil `FD_Release` build：`0 Error(s), 0 Warning(s)`。
- 生成 `103 + 309/Project/Users/Objects/FD_Release.bin`，大小 61100 bytes。

## 2026-06-02 SystemDebug MCU 资源、耗时和喂狗快照增强

### 本次源码修改

- `SystemDebug.h`：新增 `g_dbg.rcc`、`g_dbg.irq`、`g_dbg.periph`、`g_dbg.reset` 四个子结构体，用于 Keil Watch 展开查看 MCU 时钟、中断、外设寄存器和复位来源。
- `SystemDebug.h` / `Runtime.c`：新增 `g_dbg.profile`，记录整轮主循环、前台任务、IO/低功耗/CAN 任务、后台任务和 Debug 打印的 `last_us/max_us/call_cnt`。
- `SystemDebug.h` / `System_Init.c`：新增 `g_dbg.watchdog`，记录 `IWDG_Feed()` 次数、最近喂狗 tick、最近/最大喂狗间隔、IWDG PR/RLR/SR 和 IWDG reset 标志。
- `SystemDebug.c`：新增 `SystemDebug_SnapshotMcuResources()`，在 `SystemDebug_Snapshot()` 中只读采集 RCC、NVIC/SCB/SysTick、EXTI、USART、CAN、ADC、DMA1、TIM3/TIM4、FLASH、PWR 关键寄存器。
- `SystemDebug.c`：外设寄存器读取前先检查 RCC 对应时钟使能；未使能时字段填 0，避免把“外设未开”和“状态为 0”混淆。
- `System_Monitor.c`：修复删除 `ERROR_CAN` 后基础错误 offset 表未同步的问题，并将 `ERROR_REMOVE_*` / `ERROR_STATUS_*` 改为显式映射，避免枚举顺序变化导致错误位错读/错清。

### 安全约束

- 未读 USART `DR`、CAN FIFO 数据寄存器等有副作用的寄存器。
- 未清除 pending、reset、错误标志；`RCC->CSR` 仅做快照，复位来源仍保留给调试观察。
- 仅在 `PROJECT_CFG_DEBUG_MONITOR_ENABLE` 下生效，关闭该宏时 `SystemDebug` 为空实现。
- `ERROR_REMOVE_CAN` / `ERROR_STATUS_CAN` 当前不映射到任何基础错误位，符合去掉 CAN 通信错误位的方向，不会错映射到 EEPROM。

### 本次验证

- Keil `FD_Release` build：`0 Error(s), 0 Warning(s)`。
- 生成 `103 + 309/Project/Users/Objects/FD_Release.bin`，大小 59768 bytes。

### 兼容性说明

- 未修改 Modbus/CAN 协议、寄存器地址、CAN ID、payload、IAP 地址、AFE 参数和 SOC 算法。
- `g_dbg` 结构体布局发生扩展，仅用于调试 Watch；不作为对外通信协议。

## 2026-06-02 LedBar 初始化回归修复

### 本次源码修改

- `LedBar.c`：恢复 `LedBar_Init()` 的一次性初始化保护，避免 `APP_LedBar()` 每轮主循环重置显示窗口、按键滤波、扫描帧和 TIM4 状态。
- `LedBar.c`：恢复 TIM4 扫描定时器初始化状态保护，非空显示帧更新时不重复重配 TIM4，空帧/STOP 前仍会关闭扫描定时器和 GPIO。
- `LedBar.c`：恢复按键和 `MCU_WK` 二值滤波的首次采样预置，避免启动时把已有电平误判为新边沿。
- `SystemDebug.c`：保留 `g_dbg.soc.init_over` 调试字段布局，并在当前运行快照中固定填充为 1，避免打印未更新的旧值。

### 问题根因

`02bb091` 删除初始化完成类变量时，把 LedBar 运行态保护一并删除，导致 `Runtime_RunOnce()` 每轮调用 `APP_LedBar()` 时都会重新执行 `LedBar_Init()`。这会让 `startup_display_armed` 和 `soc_display_10ms` 无法自然保持/归零，表现为数码管持续闪烁，并可能让 `LP_BLOCK_LED_ACTIVE` 长时间阻塞低功耗。

### 本次验证

- Keil `FD_Release` rebuild：`0 Error(s), 0 Warning(s)`。
- 生成 `103 + 309/Project/Users/Objects/FD_Release.bin`，大小 58360 bytes。
- 上电后数码管只在启动显示窗口内显示，不应因为主循环重复初始化而持续闪烁。
- 单击/唤醒显示 SOC 后，显示窗口结束应熄屏并释放 `LP_BLOCK_LED_ACTIVE`。

### 兼容性说明

- 未修改 Modbus/CAN 协议、寄存器地址、CAN ID、payload、IAP 地址、AFE 参数和 SOC 算法。
- 未改变启动/按键/唤醒显示窗口时长，只恢复运行态保持。

## 2026-05-27 RTC/STOP 低功耗进不去修复

### 本次源码修改

- `LedBar.c`：修复 `LedBar_IsActiveForLowPower()`，`startup_display_armed` 只作为启动显示已触发标志，不再作为低功耗阻塞条件；真实显示窗口、帧扫描和扫描定时器仍会阻塞 STOP，保留用户显示体验。
- `System_Init.c` / `System_Init.h`：`EnableLowPowerDebug()` 在 Debug 构建打开低功耗调试保持，在 Release 构建显式清除 `DBGMCU_CR_DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP`。
- `AppInit.c`：启动阶段统一调用 `EnableLowPowerDebug()`，避免 Release 继承调试器残留的 DBGMCU 低功耗调试位。

### 本次验证

- Keil `FD_Release` rebuild：`0 Error(s), 0 Warning(s)`。
- 安全脚本烧录 App 到 `0x08004800`，未覆盖 IAP。
- ST-Link 读取确认 Release 下 `DBGMCU_CR = 0x00000000`。
- Release 继续运行后普通 ST-Link attach 失败，符合目标进入 STOP 且 DBG_STOP 关闭后的预期。

### 兼容性说明

- 未修改 Modbus/CAN 协议、CAN ID、payload、IAP 入口、AFE 保护配置和参数存储格式。
- 未关闭启动/唤醒 SOC 显示窗口，只修复窗口结束后的低功耗释放。

## 2026-05-27 ST-Link BMS 长期监控工具

### 本次新增

- `tools/stlink_bms_monitor.ps1`：长期监控板子、MCU 和 BMS 低功耗状态，支持 `ReleaseProxy` 和 `DebugProbe` 两种模式。
- `docs/STLINK_BMS_MONITOR_2026-05-27.md`：记录工具用途、命令、输出字段和 RTC STOP 检测限制。

### 设计说明

- `ReleaseProxy` 不打开 DBG_STOP，适合真实功耗监控，通过 ST-Link attach 失败判断目标大概率处于 STOP 或调试域关闭。
- `DebugProbe` 会尝试临时打开 `DBGMCU_CR_DBG_STOP`，用于在 RTC STOP 中读取 RAM 状态，但不适合作为功耗实测依据。

### 本次验证

- `ReleaseProxy -Count 1` 可正常输出 `LOW_POWER_OR_DBG_OFF` 或 `TIMEOUT_LOW_POWER_OR_DBG_OFF` 并生成 CSV/summary。
- `DebugProbe -Count 1 -DebugPrepareAttempts 1` 在板子已处于 Release STOP 时不会卡死，会提示需要唤醒/复位后再接入诊断监控。

### 后续增强

- attach 成功时额外读取 MCU ID、RCC/PWR/SCB fault 寄存器、BMS 电压/电流/SOC/SOH/fault、Type-C 电流、AFE 采样序号、Flash 写入标志、老化状态和低功耗参数。

## 2026-05-27 App_Can 低功耗优化

### 本次源码修改

- `Can_HDX.c`：关闭 bxCAN 自动重发，`CAN_NART = ENABLE`，避免无 ACK 时持续重发。
- `Can_HDX.c`：新增 CAN 收发器按需上电/断电，发送前等待 `PROJECT_CFG_CAN_POWER_STABLE_TICKS`，空闲后关闭 `GPIO_CMNT_EN`。
- `Can_HDX.c`：新增 no-ACK 退避；连续失败或超时达到阈值后停止完整业务广播，只保留轻量探测。
- `CanFeidaoFrames.h`：RTC/idle probe 探测帧缩减为单帧 `CAN_FEIDAO_MSG_VOLTAGE_CURRENT_1000MS`。
- `app_lowpower.c`：低功耗框架不再因 CAN bus active 永久阻塞 RTC STOP，只在 CAN busy 时阻塞。

### 本次文档更新

- `docs/CAN_APP_LOW_POWER_OPTIMIZATION_2026-05-27.md`

### 兼容性说明

- 未修改 CAN ID、payload、App 命令、Modbus 桥接、IAP 入口和老化时间广播含义。
- active 总线仍保持 1000 ms / 5000 ms 完整周期广播；idle 总线默认 10 s 轻量探测。

## 2026-05-27 CAN 低功耗配置与 idle sleep 预留

### 本次源码修改

- `Can_HDX.c`：将 RTC 唤醒 CAN 广播周期改为 `PROJECT_CFG_CAN_RTC_WAKE_PERIOD_SECONDS`，默认 `1s`，保持当前客户可见行为。
- `Can_HDX.c`：新增 CAN active 超时保持，`PROJECT_CFG_CAN_BUS_ACTIVE_HOLD_SECONDS` 默认 `10s`；最后一次 TX ACK 或 RX 帧后超时清除 active，避免历史 CAN active 永久阻塞低功耗。
- `Can_HDX.c`：CAN active 标志和时间戳按中断/主循环共享状态处理。
- `Runtime.c`：预留 STM32 运行态 idle sleep (`WFI`) 入口，只在系统 tick、SCI、CAN、Flash、IAP/参数写入均空闲时进入。
- `Project_Config.h`：新增 CAN 低功耗和 idle sleep 配置项；`PROJECT_CFG_IDLE_SLEEP_ENABLE` 默认 `0`，待硬件实测后再决定是否量产打开。

### 本次文档更新

- `docs/protocol/can_protocol.md`
- `docs/design/low_power_design.md`
- `docs/changelog/change_log.md`

### 兼容性说明

- 未修改 CAN ID、payload、App 命令、Modbus 桥接语义、IAP 入口。
- 未修改保护逻辑、MOS 控制、AFE 配置、参数存储格式。
- CAN RTC 周期广播默认仍为 `1s`。

## 2026-05-27 文档清理与合并

### 本次删除

- 删除确认已合并、过时、重复、临时或旧方案性质的 Markdown 文档 115 份。
- 删除 `TEST_PENDING copy.md`，保留 `TEST_PENDING.md`。
- 没有保留 `docs/archive/old_docs/` 旧文档副本；如需找回旧文档，可从 Git 历史恢复。

删除依据和完整清单见：

- `docs/review/document_cleanup_report.md`

### 本次新增

- `docs/protocol/modbus_register_map.md`
- `docs/protocol/can_protocol.md`
- `docs/protocol/uart_protocol.md`
- `docs/design/led_display_design.md`
- `docs/design/bootloader_iap_design.md`

### 本次更新

- `docs/README.md`
- `docs/archive/README.md`
- `docs/changelog/change_log.md`

### 本次源码修改

没有修改源码。

### 暂不删除

以下类型文档保留为 `NEED_CONFIRM`：

- 被 `tools/project_check.py` 固定引用的旧文档。
- 根目录协作、发布、调试、TODO 类流程文档。
- comm tool 子工程文档。
- 当前工作区已有用户修改的文档。

## 2026-05-26 文档体系整理

### 本次新增

- `docs/README.md`
- `docs/project_overview.md`
- `docs/architecture.md`
- `docs/module_map.md`
- `docs/design/storage_design.md`
- `docs/design/protocol_design.md`
- `docs/design/soc_design.md`
- `docs/design/adc_afe_design.md`
- `docs/design/low_power_design.md`
- `docs/test/test_plan.md`
- `docs/archive/README.md`
- `docs/review/document_inventory.md`
- `docs/review/document_source_consistency.md`
- `docs/review/document_duplicate_analysis.md`
- `docs/review/document_structure_plan.md`
- `docs/review/document_merge_plan.md`

### 本次源码修改

没有修改源码。

### 本次合并内容

本轮只做低风险“内容合并”和“权威入口创建”，没有移动、删除旧文档。

已合并到权威文档的主题：

- 项目总览和架构。
- Flash/EEPROM 兼容层和后 64K 存储。
- Modbus/CAN 通信关系。
- SOC 当前算法链路。
- ADC/AFE 当前数据流。
- RTC/低功耗/IWDG 当前行为。
- 全项目测试计划。

### 仍需确认

1. 是否归档旧低功耗阶段文档。
2. 是否归档旧 CAN 低功耗文档。
3. 是否归档旧外部 EEPROM 布局文档。
4. 是否后续删除 `TEST_PENDING copy.md`。
5. 是否补建 `docs/protocol/*`, `docs/design/led_display_design.md`, `docs/design/bootloader_iap_design.md`。
