# RTC Sleep Port 分层重构说明

日期：2026-05-25

## 目标

`rtc_sleep.c` 只保留低功耗状态机和策略判断，不直接读取 GPIO、RTC、CAN、AFE 寄存器，也不使用 `AFE_TYPE` 之类的宏在核心文件里选择不同 AFE 分支。

当前 RTC sleep 分层文件：

- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep.h`
- `103 + 309/Project/Source/rtc_sleep_port.h`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/rtc_sleep_afe_port.h`
- `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c`

## 分层边界

### `rtc_sleep.c`

职责：

- 管理 `g_stLowPowerRtcStatus.mode` 和 `readyToSleep`。
- 判断低压深睡、普通 RTC STOP 周期休眠和阻塞原因。
- 维护本轮 RTC STOP 累计秒数和 RTC 唤醒轮次。
- 调用 port 接口完成采样、STOP、CAN/SOC 服务和唤醒源识别。

禁止新增：

- `AFE_TYPE`、`sh36xx`、`bq76xx_afe` 这类 AFE 选择分支。
- `GPIO_ReadInputDataBit()`、`MTPRead()`、`UpdateVoltageFromBqMaximo()`、`RTC_*()`、`Sys_StopMode()` 等底层调用。
- 具体 AFE 寄存器结构体访问。

### `rtc_sleep_port.c`

职责：

- 适配当前 MCU、RTC、STOP、GPIO 唤醒脚、CAN RTC 服务、SOC 休眠补偿、日志和复位休眠提交。
- 通过 `rtc_sleep_afe_port.h` 调用 AFE 适配层，不直接写 `AFE_TYPE` 分支。
- 迁移到其它 MCU 时，优先新增或替换 MCU port 文件，不改 `rtc_sleep.c`。

### `rtc_sleep_afe_port.h` / `rtc_sleep_afe_sh367309.c`

职责：

- `rtc_sleep_afe_port.h` 定义 AFE 适配接口。
- `rtc_sleep_afe_sh367309.c` 是当前 SH367309 的 AFE 实现，负责 AFE 数据刷新、休眠阻塞、当前唤醒、AFE 唤醒判断。
- 迁移到其它 AFE 时，新增同接口实现文件，并在 Keil 工程中替换源文件；不要在 `rtc_sleep.c` 或 `rtc_sleep_port.c` 追加 `AFE_TYPE` 分支。

## 接口分类

| 类别 | 接口示例 | 说明 |
| --- | --- | --- |
| 时间和状态 | `RtcSleep_PortIsOneSecondTick()`、`RtcSleep_PortGetIdleDelayTargetSeconds()` | 核心层不直接访问 `g_st_SysTimeFlag` 或 `sys_time` |
| 电池测量 | `RtcSleep_PortGetCellMinMv()`、`RtcSleep_PortGetChargeCurrentMa()` | 核心层只读取抽象测量值 |
| AFE 判断 | `RtcSleep_PortUpdateRtcData()`、`RtcSleep_PortIsAfeSleepBlocked()` | MCU port 转发到 AFE port，AFE 差异由源文件边界处理 |
| MCU/RTC | `RtcSleep_PortPrepareRtcStop()`、`RtcSleep_PortEnterStop()` | STOP 和 RTC Alarm 配置隐藏在 MCU port 内 |
| 业务服务 | `RtcSleep_PortRunCanRtcWakeService()`、`RtcSleep_PortApplySocRtcRest()` | CAN/SOC 业务不侵入核心状态机 |
| 唤醒源 | `RtcSleep_PortHasCurrentWake()`、`RtcSleep_PortGuessWakeupSource()` | 核心层只接收 `enum irqWakeup` |

## 移植步骤

1. 保持 `rtc_sleep.c` 不改。
2. 新 MCU：实现一份同接口的 `rtc_sleep_port.c`，或将当前文件复制为新平台文件后在工程里替换。
3. 新 AFE：实现一份同接口的 `rtc_sleep_afe_xxx.c`，只替换 Keil 工程中的 AFE port 源文件。
4. 确认 `RtcSleep_PortPrepareRtcStop()` 和 `RtcSleep_PortRestoreAfterStop()` 完整覆盖新 MCU 的时钟、GPIO、ADC、AFE、CAN 恢复顺序。
5. 确认 `RtcSleep_AfePortIsSleepBlocked()` 能表达新 AFE 是否允许进入 RTC STOP。
6. 编译 `FD_Release`，并确认 App 仍从 `0x08004800` 构建。

## 自动约束

`tools/project_check.py` 已加入 RTC sleep 分层约束：

- `rtc_sleep.c` 不允许出现底层 MCU/AFE token。
- `rtc_sleep_port.c` 不允许通过 `AFE_TYPE` 分支选择不同 AFE。
- 当前 Keil 工程必须同时包含 `rtc_sleep_port.c` 和 `rtc_sleep_afe_sh367309.c`。

## 验证重点

- 空闲达到 `RtcSleep_PortGetIdleDelayTargetSeconds()` 后能进入 RTC STOP。
- `RTC_IT_ALR + EXTI17` 周期唤醒后 CAN/SOC 服务仍运行。
- MCU 唤醒脚、充放电电流、AFE 故障、工厂老化、外部通信会阻止 RTC STOP。
- 低压深睡优先级不被普通阻塞条件覆盖。
