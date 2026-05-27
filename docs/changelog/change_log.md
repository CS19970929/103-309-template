# 文档变更记录

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：当前主工程源码和 `docs/review/*`
最后更新时间：2026-05-27
未确认事项：`NEED_CONFIRM` 文档仍需用户确认是否保留；部分旧文档仍被 `tools/project_check.py` 固定引用。

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
