# ST-Link BMS 长期监控工具

状态：已按源码验证

日期：2026-05-27

参考源码和工具：

- `tools/stlink_bms_monitor.ps1`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/app_lowpower.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/System_Init.c`

## 目标

提供一个可长期运行的 ST-Link 监控工具，用于观察板子、MCU 和 BMS 低功耗相关状态。

工具支持两种模式：

| 模式 | 用途 | 是否适合功耗实测 | 能否在 RTC STOP 内读 RAM |
|---|---|---|---|
| `ReleaseProxy` | 真实 Release 低功耗监控；通过 ST-Link attach 成功/失败判断 RUN/STOP 或调试关闭状态 | 是 | 否，Release 关闭 DBG_STOP 后 STOP 内读不到 |
| `DebugProbe` | 诊断模式；尝试复位运行后临时打开 DBGMCU 低功耗调试位，再持续读取 RAM 状态 | 否 | 是，但会抬高功耗 |

## 常用命令

真实低功耗监控，跑 10 分钟，每 10 秒采样一次：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\stlink_bms_monitor.ps1 `
  -Mode ReleaseProxy `
  -DurationMinutes 10 `
  -IntervalSeconds 10
```

固定采样 60 次：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\stlink_bms_monitor.ps1 `
  -Mode ReleaseProxy `
  -Count 60 `
  -IntervalSeconds 10
```

诊断模式，允许在 RTC STOP 中读取 RAM 状态：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\stlink_bms_monitor.ps1 `
  -Mode DebugProbe `
  -Count 60 `
  -IntervalSeconds 10
```

如果板子已经处于 Release STOP，且 `DBGMCU_CR_DBG_STOP` 已关闭，`DebugProbe` 可能无法立即接入。此时需要按键/充电器/复位让 MCU 回到 RUN 窗口，工具才能临时写入 DBGMCU 低功耗调试位。

## 输出

默认输出目录：

```text
logs/stlink_bms_monitor/
```

主要文件：

- `stlink_bms_monitor_*.csv`：长期采样数据。
- `stlink_bms_monitor_*.summary.txt`：采样汇总。
- `last_status.json`：最近一次采样状态。
- `raw/`：使用 `-KeepRawLogs` 时保存 OpenOCD 原始输出。

CSV 重点字段：

| 字段 | 说明 |
|---|---|
| `Result` | `ATTACH_OK_RUN_OR_WAKE`、`ATTACH_OK_DEBUG_HOLD`、`LOW_POWER_OR_DBG_OFF`、`TIMEOUT_LOW_POWER_OR_DBG_OFF` 等 |
| `RtcMode` | `g_stLowPowerRtcStatus.mode` |
| `RtcBlock` | `g_stLowPowerRtcStatus.blockReason` |
| `LpBlockReason` | `LP_BuildBlockReason()` 的 bitmask |
| `LedSocWindow10ms` | LED/SOC 显示窗口剩余 10ms tick |
| `LedStartupArmed` | 启动显示是否已触发 |
| `DbgmcuCr` | `DBGMCU_CR` 当前值 |
| `McuDevId` / `McuRevId` | STM32 DBGMCU IDCODE 解析出的 MCU 型号和修订 |
| `RccCr` / `RccCfgr` / `PwrCr` / `PwrCsr` | MCU 时钟和电源控制关键寄存器 |
| `ScbIcsr` / `ScbCfsr` / `ScbHfsr` | Cortex-M 异常和 fault 状态寄存器 |
| `CellVMax_mV` / `CellVMin_mV` / `CellVDelta_mV` | BMS 单体最高、最低和压差 |
| `PackVoltage_10mV` | BMS 总压，单位 10mV |
| `Ichg_A10` / `Idsg_A10` | 充/放电电流，单位 0.1A |
| `SocPercent` / `SohPercent` | SOC/SOH 百分比 |
| `FaultThird` | 当前保护 fault bitmask |
| `TypeCOutCurrent_mA` / `Vbat_mV` | Type-C 输出电流和 ADC 总压路径 |
| `AfeSampleSeq` | AFE 电流采样序号 |
| `FlashUpdateFlag` / `FlashUpdateE2prom` | Flash/IAP/参数写入活动标志 |
| `FactoryAgingState` | 老化状态 |
| `SleepVNormal_mV` / `SleepVLow_mV` / `SleepTimeRtc_min` | 低功耗相关参数 |

## 判断规则

- `ReleaseProxy` 下长时间出现 `LOW_POWER_OR_DBG_OFF` 或 `TIMEOUT_LOW_POWER_OR_DBG_OFF`，通常说明 MCU 已在 STOP 或调试域关闭，符合 Release 低功耗预期。
- `ReleaseProxy` 下频繁出现 `ATTACH_OK_RUN_OR_WAKE`，需要检查是否有通信、LED、Flash、故障、按键、充放电等阻塞。
- `DebugProbe` 下出现 `ATTACH_OK_DEBUG_HOLD`，说明当前已经进入调试保持状态，可以读取 RTC/LP/LedBar RAM 状态，但不能使用此时的电流作为功耗结果。
- MCU/BMS 详细字段只有 attach 成功时才有值；Release 已经停在 RTC STOP 且 DBG_STOP 关闭时，这些字段会为空。

## 已验证

2026-05-27：

- `ReleaseProxy -Count 1` 在当前已进入 STOP 的板子上输出 `LOW_POWER_OR_DBG_OFF` 或 `TIMEOUT_LOW_POWER_OR_DBG_OFF`，脚本正常汇总。
- `DebugProbe -Count 1 -DebugPrepareAttempts 1` 在当前已进入 Release STOP 的板子上提示准备失败并继续记录 `LOW_POWER_OR_DBG_OFF`，不会卡死。
- 2026-05-27 后续增强：attach 成功时可额外读取 MCU 寄存器、BMS 电压/电流/SOC/fault、AFE 采样序号、Flash 写入标志、老化状态和低功耗参数。
