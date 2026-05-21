# CAN 模块精简记录 - 2026-05-15

## 目标

继续简化当前 CAN 架构，让代码更清晰、方便阅读和维护。本轮仍按行为等价处理：不修改飞道 CAN 报文协议、不修改发送周期、不修改低功耗策略、不修改 CAN 波特率和 IAP/App 地址安全规则。

## 当前边界

- `Can_HDX.c`：CAN 硬件初始化、收发器电源控制、周期调度、BusOff 监控、No-ACK 统计、RTC 唤醒服务。
- `CanFeidaoFrames.c/.h`：飞道扩展帧的报文字段组包、报文发送顺序、周期报文掩码。
- `Can_HDX_Transmit()`：CAN 运行时对协议帧模块开放的唯一发送出口，保留原有标准帧地址补偿、无邮箱统计和 `ERROR_CAN` 回调。

## 2026-05-21 低功耗与 BusOff 策略更新

- 发送批次完成后，CAN 运行时同时关闭 `GPIO_CMNT_EN` 收发器电源，并请求 bxCAN 外设进入 sleep。
- 下一次发送前先唤醒 bxCAN 外设，再打开收发器电源并等待 `100ms` 稳定窗口。
- `CAN_ABOM` 改为 `ENABLE`，BusOff 由 bxCAN 硬件自动恢复。
- `Can_BusOFF_Monitor()` 保留为监控入口：只负责 BusOff 计数、错误快照和恢复后 `500ms` 稳定确认，不再手动设置/清除 `CAN_MCR_INRQ`。
- `CAN_NART` 仍保持 `ENABLE`，无 ACK 时不自动重发，继续配合 No-ACK 统计和空闲探测降低未接设备功耗。

## 本轮变化

- 新增 `CanFeidaoFrames.c/.h`，将以下飞道报文从 `Can_HDX.c` 移出：
  - 1000 ms：总压/电流、SOC。
  - 5000 ms：容量、SOH、版本、状态、出厂时间。
- 新增 `CanFeidao_SendNextPending()`，由协议帧模块维护发送优先级，`Can_HDX.c` 不再直接关心每个报文的组包函数。
- 新增 `CAN_FEIDAO_1000MS_MSG_MASK`、`CAN_FEIDAO_5000MS_MSG_MASK`、`CAN_FEIDAO_RTC_PROBE_MSG_MASK`，让调度代码只表达“本周期发哪些帧”。
- Keil `FD_Release`、`FD_Debug` 两个 Target 都加入 `CanFeidaoFrames.c`。
- `Can_HDX.c` 删除了协议字段组包细节，主职责收口为 CAN 运行时和低功耗边界。

## 保留行为

- 飞道扩展帧 ID 基址仍为 `0x14F80200`，通道号保持原顺序。
- 发送顺序保持：总压/电流 -> SOC -> 容量 -> SOH -> 版本 -> 状态 -> 出厂时间。
- 1000 ms、5000 ms 周期和 RTC 空闲探测逻辑不变。
- `CAN_NART` 仍保持启用，避免无外部 CAN 设备时反复重发。
- `CAN_ABOM` 启用，BusOff 恢复由硬件根据总线空闲序列完成，软件只记录状态。
- CAN 波特率仍为 250 kbit/s。
- App 地址仍为 `0x08004800`，IAP 地址仍为 `0x08000000`。
- 本轮不烧录、不读取 COM4。

## 验证

固定执行：

```powershell
py -3.9 tools\project_check.py --quiet
py -3.9 tools\soc_replay_test.py
py -3.9 tools\run_soc_host_c_test.py
.\tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release
```

构建后检查：

- `FD_Release` 无 CAN 模块新增 warning/error。
- 本轮构建结果：`FD_Release.bin=63496B`，低于早前记录的主循环收口基线 `64436B`。
- 当前 Release 构建仍有 6 个既有 warning，位置在 `DataDeal.c`、`PubFunc.c`、`SH367309_Func.c`、`rtc_sleep.c`，不来自本轮新增的 `CanFeidaoFrames.c` 或 `Can_HDX.c`。
- `Can_HDX.c` 中不再出现飞道帧字段组包函数。

## 后续方向

- 若继续优化 CAN，可再把 `feidao_can_*` 运行时状态拆为 `CanRuntime` 风格命名，降低历史命名噪声。
- 若继续优化低功耗，可将 RTC 唤醒服务、No-ACK 空闲探测和收发器电源状态机拆成独立小模块。
- 若需要改协议字段或新增报文，应优先只修改 `CanFeidaoFrames.c/.h`，避免把协议细节重新扩散到运行时模块。
