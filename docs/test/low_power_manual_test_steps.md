# RTC 低功耗手工测试步骤

本文档由 TestAgent 在第一阶段生成。只新增测试文档，未修改源码。步骤面向后续实现 `Stop + RTC 周期唤醒` 最小框架后的上板验证，也可用于当前已有 `rtc_sleep` 路径的回归。

## 1. 安全准备

1. 确认当前工作目录是仓库根目录：

   ```powershell
   Set-Location "E:\TODO\103 + 309 - 副本"
   ```

2. 确认 App 安全烧录脚本只指向 `0x08004800`：

   ```powershell
   .\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin"
   ```

   期望输出包含：
   - `address: 0x08004800`
   - `IAP stays at 0x08000000`
   - dry-run 未真正烧录。

3. 需要真实烧录时只能使用：

   ```powershell
   .\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash
   ```

   禁止把 `FD_Debug.bin` 或 `FD_Release.bin` 裸写到 `0x08000000`。

4. 低功耗测试建议设备：
   - 可限流电源或电池模拟器。
   - 串口工具或 USB-RS485，默认 `COM4/19200/slave=1`。
   - 如果走 comm tool/CAN：comm tool 串口默认 `COM4/115200`。
   - 电流表或功耗分析仪。
   - 可选：Keil Watch，用于观察 `g_stLowPowerRtcStatus`、`sys_time.rtc_alm_cnt`、`is_rtc_wakekup`、`g_stCanLowPowerStatus`。

## 2. 基线读取

1. 直接 Modbus 读取板端状态：

   ```powershell
   py -3.9 tools\soc_online_monitor.py --port COM4 --baud 19200 --slave 1 --samples 30 --interval 1
   ```

2. 记录至少这些字段：
   - `vmax_mv/vmin_mv/vdelta_mv`
   - `total_v_x100`
   - `ichg_a10/idsg_a10`
   - `soc/soh`
   - `fault1/fault2/fault3`
   - `balance1/balance2`

3. 读取 SOC 测试状态，确认量产隔离：

   ```powershell
   py -3.9 tools\soc_online_monitor.py --port COM4 --baud 19200 --slave 1 --samples 1 --interval 1
   ```

   或使用 SOC UI：

   ```powershell
   .\tools\start_soc_test_ui.ps1
   ```

   量产固件 `0xD300 supported=0` 是正常结果，表示 MCU 注入式 SOC 测试入口关闭。

## 3. RTC/Stop 主链路

1. 让板端处于空闲条件：
   - 无充电和放电电流，`u16Ichg/u16IDischg` 应接近 0。
   - 不持续发送 Modbus/CAN。
   - 老化模式未运行。
   - 无 AFE 故障、MOS 异常或保护事件。

2. 等待低功耗策略进入 HICCUP/RTC 路径。源码依据：
   - `rtc_sleep.c:220-225` 空闲计时到 `sys_time.time_enter_rtc` 后 `entersleep(HICCUP_MODE)`。
   - `rtc_sleep_port.c:108-116` 配置 RTC Stop。
   - `conf.c:374-385` 进入 Stop。

3. 观察通过条件：
   - 电流明显下降。
   - `RTC.c:518-520` 的 `RTCAlarm_IRQHandler()` 能触发。
   - `RTC.c:511-515` 设置 `is_rtc_wakekup=true` 并累加 `sys_time.rtc_alm_cnt`。
   - `rtc_sleep.c:313-327` 统计 `rtc_elapsed_seconds`，执行 SOC RTC 补偿和 CAN RTC 服务。

4. 失败时记录：
   - 是否进入 Stop 前就被 `LOW_POWER_RTC_BLOCK_*` 阻塞。
   - `RTC_IT_ALR`、`EXTI_Line17`、`RTCAlarm_IRQn` 是否有 pending。
   - 是否 Stop 后只醒一次，随后不能再次入睡。

## 4. IWDG 验证

1. 当前量产配置下 `PROJECT_CFG_WDOG_ENABLE=1`、`PROJECT_CFG_RTC_ENABLE=1`，`Init_IWDG()` 使用 `IWDG_Prescaler_256 + Reload 0x0FFF`。

2. 空闲 RTC Stop 循环运行 10 分钟：
   - 不操作串口。
   - 不接入充放电。
   - 记录是否发生重启。

3. 通过标准：
   - 不发生 IWDG 误复位。
   - Stop 前后喂狗路径正常：`rtc_sleep_port.c:118-123`，`Can_RtcWakeService()` 循环内也有喂狗。

4. 后续如果修改 RTC 周期超过 1s，必须增加专项边界测试：
   - RTC 唤醒周期必须小于 IWDG 最短超时，并预留恢复时钟、CAN 服务、Flash 保存和业务任务时间。
   - 如果框架判断不安全，应阻塞睡眠并暴露 `LP_BLOCK_IWDG_UNSAFE`。

## 5. Stop 唤醒后时钟和任务恢复

1. Stop 唤醒后立即读取 `0xD000`：

   ```powershell
   py -3.9 tools\soc_online_monitor.py --port COM4 --baud 19200 --slave 1 --samples 10 --interval 1
   ```

2. 观察通过标准：
   - 串口读无超时、无 CRC 错。
   - ADC 数据继续变化或保持合理值。
   - SOC、故障字、状态字可读。
   - 主循环没有因为 `SysTick` 或 TIM3 恢复异常卡死。

3. 源码检查点：
   - `conf.c:384` Stop 返回后调用 `cpu_frequency_conf()`。
   - `rtc_sleep_port.c:207-211` `cpu_frequency_conf()` 调用 `SystemInit()`、`SystemCoreClockUpdate()`、`InitDelay()`。
   - `conf.c:392-421` 恢复 Delay/RTC/IO/ADC/SCI/CAN/TIM3/AFE IIC。

## 6. CAN 回归

1. 启动用户上位机：

   ```powershell
   .\tools\start_comm_tool_upgrade_ui.ps1 -Port COM4 -Baud 115200
   ```

2. 或命令行读取 BMS：

   ```powershell
   py -3.9 tools\comm_tool_host.py bms-read --port COM4 --baud 115200 --address 0xD000 --count 63 --long-timeout 90
   ```

3. 读取 CAN 诊断：

   ```powershell
   py -3.9 tools\comm_tool_host.py can-diag --port COM4 --baud 115200
   ```

4. 测试场景：
   - 有 CAN 设备连接，空闲进入 RTC Stop。
   - 断开 CAN 设备，观察无 ACK/timeout 是否可控。
   - 再接入 CAN 设备，确认通信恢复。

5. 通过标准：
   - `Can_PrepareSleep()` 会取消待发队列并关闭 `GPIO_CMNT_EN`。
   - `Can_RtcWakeService()` 醒后重新打开 `GPIO_CMNT_EN`，服务到期报文，最多等待 `FEIDAO_CAN_RTC_SERVICE_TIMEOUT_TICKS=150`。
   - UI 实时监控和 CAN 诊断不出现持续 busoff/timeout。

## 7. Modbus/通信活跃禁止休眠

1. 连续读取 `0xD000`：

   ```powershell
   py -3.9 tools\soc_online_monitor.py --port COM4 --baud 19200 --slave 1 --samples 120 --interval 0.5
   ```

2. 同时观察低功耗状态：
   - `Sci_Upper.c:1478` 每收到一个字节递增 `RTC_ExtComCnt`。
   - `rtc_sleep.c:201-207` 外部通信计数变化后应产生 `LOW_POWER_RTC_BLOCK_EXT_COMM`。
   - `Sci_Upper.c:1678-1690` 的 `Sci_IsAnyPortBusy()` 可作为后续统一 `LP_BLOCK_COMM` 接入点。

3. 通过标准：
   - 连续通信期间不进入 Stop。
   - 读写响应完整，不出现半包。
   - 停止通信后，空闲延时到期再允许进入 RTC Stop。

## 8. ADC/AFE/SOC 回归

1. ADC：
   - Stop 前记录 `0xD000` 中电压、电流。
   - RTC 唤醒后连续读取 30 个样本。
   - 通过标准：`ADC_StopForLowPower()` 后 `InitADC()` 恢复，电压/电流不冻结、不全 0。

2. AFE：
   - 正常状态下允许进入 RTC Stop。
   - 模拟 AFE 状态异常、故障或 MOS 关闭。
   - 通过标准：`RtcSleep_AfePortIsSleepBlocked()` 或 `RtcSleep_AfePortHasAfeWake()` 阻塞/退出睡眠，保护状态不丢。

3. SOC：
   - 睡前记录 SOC、电压和时间。
   - 运行 RTC 周期唤醒至少 10 次。
   - 通过标准：`RtcSleep_PortApplySocRtcRest()` 调用 `SOC_ApplyRtcRelaxationCompensation(rest_seconds, vmin, vmax)`，SOC 不异常跳变。

## 9. Flash 写入相关测试

当前源码 Flash 保存路径包括：
- `StorageFlash_SaveSocData()`
- `StorageFlash_SaveAfeData()`
- `StorageFlash_SaveRwParamData()`
- `StorageFlash_SaveLogData()`
- `StorageFlash_SaveFactoryAgingData()`

手工验证步骤：

1. 使用 UI 或命令行执行一次非破坏性写入，例如写一次 SOC：

   ```powershell
   py -3.9 tools\comm_tool_host.py bms-write-soc --port COM4 --baud 115200 --soc 80 --long-timeout 90
   ```

2. 写入期间观察是否进入 Stop。

3. 写完后断电重启，再读取状态确认保存结果。

4. 通过标准：
   - 写响应完成。
   - Flash 保存完成后再允许睡眠。
   - 重启后数据一致。

5. 当前结论：
   - 第一阶段只读发现当前尚无统一 `StorageFlash_IsBusy()`；后续最小实现应补 Flash busy 阻塞，否则该项只能通过现象回归，不能精确证明。

## 10. LED/按键/充电唤醒

1. LED 睡前：
   - 按键触发 SOC 显示。
   - 等待准备睡眠。
   - 通过标准：`LedBar_SaveSleepSoc()` 保存睡前 SOC，`LedBar_PrepareForStop()` 关闭显示并准备 GPIO。

2. 按键唤醒：
   - 睡眠中短按只显示 SOC。
   - 长按满足 `SleepDeal.c:9` 的窗口后唤醒。
   - 通过标准：显示窗口结束后能重新回低功耗，长按能恢复运行。

3. 充电唤醒：
   - 睡眠中拉低 `GPIO_CHG_IN/PIN_CHG_IN`。
   - 通过标准：`SleepDeal.c:27-30` 或 `rtc_sleep_port.c:192-197` 识别为充电唤醒；醒后 Modbus/CAN 可用。

## 11. 过放深休眠

1. 只使用电池模拟器或可限流电源，禁止用真实电芯做破坏性过放。

2. 设置条件：
   - `VCellMin <= 2800mV`。
   - 充电电流小于 `LOW_POWER_DEEP_SLEEP_ICHG_LIMIT=5mA`。
   - 保持 60s。

3. 通过标准：
   - `rtc_sleep.c:154-162` 强制深休眠路径触发 `DEEP_MODE`。
   - `SleepDeal.c:220-230` 进入 DEEP Stop 循环。
   - 接入充电后可唤醒并恢复通信。

4. 失败判据：
   - 过放被通信、老化、LED 普通显示永久阻塞。
   - 过放后 MOS/AFE 状态错误。
   - 接充电无法唤醒。

## 12. 老化和生产功能

1. 启动 comm tool UI：

   ```powershell
   .\tools\start_comm_tool_upgrade_ui.ps1 -Port COM4 -Baud 115200
   ```

2. 路径：`其它功能 -> 常用功能 -> 读取老化时间`。

3. 通过标准：
   - 老化剩余时间在原 UI 内单独可见。
   - 读取逻辑仍通过 `0x13 BMS_AGING_STATUS` 解析 `0x14F80208` 广播。
   - 老化运行时 `FactoryAging_IsActive()` 阻塞普通 RTC 睡眠。

## 13. 长稳测试

1. 完成前述 P0 项后，执行 8 小时空闲 RTC Stop 循环。

2. 每 10 分钟记录：
   - 功耗。
   - `0xD000` 状态。
   - CAN 诊断。
   - 是否复位。
   - SOC、电压、故障字、LED 状态。

3. 通过标准：
   - 无 IWDG 误复位。
   - 无通信永久失效。
   - 无 Flash 数据损坏。
   - 无 AFE/MOS 状态不同步。
   - 无 LED 常亮或扫描异常导致功耗升高。

## 14. 测试记录模板

| 项目 | 记录 |
|---|---|
| 固件文件 | `103 + 309\Project\Users\Objects\FD_Release.bin` |
| 烧录方式 | `tools\soc_flash_app_safe.ps1`，地址 `0x08004800` |
| 板卡编号 |  |
| 电源/电池模拟器设置 |  |
| 串口 | `COM4/19200/slave=1` 或 comm tool `COM4/115200` |
| CAN 设备 |  |
| 初始 `0xD000` 快照 |  |
| 初始 `0xD300` 快照 |  |
| Stop 电流 |  |
| RTC 唤醒周期 |  |
| IWDG 是否复位 |  |
| CAN 诊断结果 |  |
| Modbus 读写结果 |  |
| ADC/AFE/SOC 结果 |  |
| Flash 保存结果 |  |
| LED/按键/充电唤醒结果 |  |
| 过放深休眠结果 |  |
| 结论 |  |

