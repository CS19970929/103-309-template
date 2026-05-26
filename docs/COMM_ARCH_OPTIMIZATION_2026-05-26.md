# comm tool / BMS App / PC 上位机通讯架构优化记录

日期：2026-05-26

## 背景

现场观察到 comm tool 通讯灯闪烁节奏不稳定。灯本身不是目标问题，它暴露的是三端通讯链路在实时监控、手动读写、升级、CAN 异常叠加时可能存在同步阻塞和请求排队风险。

链路如下：

- PC 上位机通过串口和 comm tool 通讯，协议入口为 `tools/comm_tool_host.py` / `tools/comm_tool_upgrade_ui.py`。
- comm tool App 将 PC 命令转换为 CAN App 服务帧或 CAN-IAP 升级帧，入口为 `firmware/comm_tool_f103ret6/source/app/ct_app.c`、`ct_can_gateway.c`、`ct_upgrade_manager.c`。
- BMS App 在 `103 + 309/Project/Source/Can_HDX.c` 接收 CAN App 命令，并映射到 `Sci_HostReadWords()` / `Sci_HostWriteWords()` 和老化模式控制。

## 已完成优化

### PC 上位机

- 实时监控读取 `0xD000` 时，首次尝试 88 字，若降级到 63 字成功，则缓存该能力，避免每轮都先失败再降级。
- BMS 序列号、软硬件版本来源仍为 `0xC002` 的 48 个寄存器，但不再每个实时监控周期都读取；默认缓存 30 秒，目标地址变化或手动读取时刷新。
- 独立实时监控窗口在主界面实时监控运行时复用主界面快照，不再额外发起一套串口/CAN 轮询。
- BMS 日志 `0xC008` 从一次读取 100 字改为按 20 字分段读取，降低单次 CAN 块读丢帧导致整窗失败的概率；分段失败只重试当前 20 字窗口，不再回退到旧的 100 字完整窗口。
- CAN 升级前主动暂停实时监控和长期记录，降低升级期间普通读写干扰。
- 修改用户上位机后仍必须覆盖固定产物：`dist\BMS_CommTool_Upgrade_UI.exe`，不要另起 exe 名称。

### comm tool App

- 对外公开升级状态值宏，避免 PC/comm tool 对 `state=1/2/3/4` 的语义重复硬编码。
- CAN 升级进行中，只允许 `GET_INFO`、`FW_INFO`、`UPGRADE_STATUS`、`UPGRADE_ABORT`、`CAN_DIAG`、`DEBUG_LOG`。
- 升级进行中收到普通 BMS 读写、老化控制、进入 IAP、改 CAN 参数等命令时返回 `BAD_STATE`，避免普通命令阻塞升级状态机。
- BMS 块读 ACK 已收到但数据帧未收齐时，返回 `CAN_TIMEOUT`，不再泛化成 `BMS_ERROR`，并清理 CAN RX 队列，降低迟到数据帧污染下一次块读的风险。

### BMS App

- CAN App 命令接收从单条 pending 改为 4 条小队列。
- 主循环来不及处理时，短时间突发命令不会被静默覆盖。
- 休眠准备和 CAN 初始化时会清空 App 命令队列，避免跨状态处理旧命令。
- `0xC008` 事件记录窗口支持子地址分段读取，例如 `0xC008+20`、`0xC008+40`，供上位机降低单次块读压力。

## 后续建议

- comm tool 的 BMS 大块读取、老化广播读取仍是同步等待模型。后续若仍出现偶发超时，建议把这些长命令改成异步任务，PC 用状态查询读取结果。
- 建议增加可读诊断字段：UART RX overflow、CAN RX drop、BMS App 命令队列满次数、单次命令最长耗时。
- 如果现场 CAN 总线存在多主机或大量广播，建议进一步收紧 comm tool CAN 过滤，避免无关帧占满 32 条 RX 队列。
