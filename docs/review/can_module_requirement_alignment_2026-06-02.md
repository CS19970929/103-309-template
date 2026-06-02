# CAN 模块需求与实现对齐

文档状态：已按源码验证

最后更新时间：2026-06-02

主要参考源码：

- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/Can_HDX.h`
- `103 + 309/Project/Source/CanFeidaoFrames.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/conf/Project_Config.h`

## 1. 已确认需求

| Requirement ID | 需求描述 | 当前决策 | 实现状态 |
|---|---|---|---|
| CAN-REQ-001 | 保留飞道周期广播扩展帧 `0x14F80200 + chd_index` | MUST_KEEP | 保留 `CanFeidaoFrames.c` 组帧和 1000ms/5000ms 运行态调度 |
| CAN-REQ-002 | 保留 CAN App 标准帧 `0x60/0x61` | MUST_KEEP | 未修改 ID、magic、CRC、payload、命令含义 |
| CAN-REQ-003 | RTC 休眠中是否周期广播 CAN | CHANGE_NEEDED | 已删除 `Can_RtcWakeService()`，RTC 周期唤醒后不主动广播 CAN |
| CAN-REQ-004 | CAN 收发器电源 | KEEP_BUT_REFACTOR | 运行态 `InitCan()` 打开 CMNT；`Can_PrepareSleep()` 关闭 CMNT；唤醒恢复后重新打开 |
| CAN-REQ-005 | bus-off 处理 | CHANGE_NEEDED | 已删除软件 bus-off 状态机，保留 `CAN_ABOM = ENABLE` 自动恢复 |
| CAN-REQ-006 | low-risk cleanup | KEEP_BUT_REFACTOR | 删除未用变量、旧 RTC CAN 接口、运行态 active/probe/no-ACK 状态和 debug 占位字段 |

## 2. 当前 CAN 行为

### 2.1 运行态通信

- `InitCan()` 初始化 GPIO/NVIC/CAN/filter，并打开 `GPIO_CMNT_EN/PIN_CMNT_EN`。
- `App_Can()` 在主循环中调度 1000ms/5000ms 周期帧、处理 CAN App 命令、服务 TX queue 和 read-block stream。
- `Can_HDX_Transmit()` 只负责入队；返回 `0` 表示入队成功，不代表硬件已经 ACK。
- `CAN_NART = ENABLE`，无 ACK 时不做硬件无限重发；软件不再维护 no-ACK 计数、active 状态或 probe 退避。

### 2.2 RTC 休眠关系

- `Can_PrepareSleep()` 会取消当前 TX、清空 CAN App 命令队列、停止 read-block stream，并关闭 CMNT 电源。
- `rtc_sleep.c` 的 HICCUP RTC 周期唤醒后只恢复硬件、做 SOC 休眠补偿和低功耗状态刷新，不再调用 CAN 周期服务。
- `RTC_GetWakeupPeriodSeconds()` 默认使用 10s 周期；IWDG 开启时仍限制最大 10s。
- 外部唤醒或退出 RTC sleep loop 后，`InitRunAfterStopWakeup()` 会重新初始化 CAN，`InitCan()` 再打开 CMNT，通信在正常运行态恢复。

### 2.3 bus-off

- bxCAN `CAN_ABOM = ENABLE` 继续开启，硬件进入 bus-off 后会按控制器规则自动恢复。
- 软件不再保存 `s_runtime.bus_off`、不再统计 `busoff_enter_cnt/busoff_recover_cnt`，也不再因 BOFF 额外清队列。
- debug snapshot 的 `bus_off` 仍从 `CAN1->ESR & CAN_ESR_BOFF` 只读获取，便于观察当前硬件状态。

## 3. 删除和保留边界

已删除：

- `Can_RtcWakeService()`
- `Can_GetIdleRtcPeriodSeconds()`
- `Can_IsBusActive()`
- `RtcSleep_PortRunCanRtcWakeService()`
- `RtcSleep_PortGetCanRtcPeriodSeconds()`
- `RtcSleep_PortIsCanBusActive()`
- `PROJECT_CFG_CAN_RTC_WAKE_PERIOD_SECONDS`
- 软件 `feidao_can_busoff_monitor()` 和 `s_runtime.bus_off`
- 运行态 `bus_active/no_ack_cnt/probe_active/last_probe_tick`
- `PROJECT_CFG_CAN_BUS_ACTIVE_HOLD_SECONDS`
- `CAN_FEIDAO_RTC_PROBE_MSG_MASK`
- debug snapshot 保留占位字段 `rtc_svc/tx_ok_cnt/tx_fail_cnt/busoff_in_cnt/busoff_out_cnt/last_tx_id`

保留：

- 飞道周期帧 ID、周期、payload。
- CAN App `0x60/0x61` 帧格式和命令含义。
- `READ_BLOCK` 分包返回。
- `ENTER_IAP` guard 和 ACK 后延迟复位。
- `Can_IsBusy()` 作为低功耗阻塞条件，避免命令/块读/发送被 STOP 打断。
- 运行态固定 1000ms/5000ms 周期调度。

## 4. 验证重点

| 项目 | 方法 | 通过标准 |
|---|---|---|
| 编译 | Keil `FD_Release` | 0 error，确认无旧接口未定义 |
| 静态门禁 | `python3 tools/project_check.py` | CAN 简化检查通过；其它历史基线失败需单独标注 |
| RTC 休眠 | 上板测 `GPIO_CMNT_EN` | 进入 RTC STOP 前关闭，唤醒恢复后打开 |
| CAN 运行态 | CAN 抓包 | 正常运行态仍有 1000ms/5000ms 周期帧 |
| bus-off | 断线/短路/错误波特率场景 | 通信异常时可观察 ESR BOFF；恢复总线后 ABOM 自动恢复发送 |
