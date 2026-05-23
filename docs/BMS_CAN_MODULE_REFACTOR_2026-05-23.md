# BMS App CAN 模块重构说明

## 目标

当前阶段先不做 CAN 动态低功耗，优先保证功能正常、代码清晰、通信稳定：

- CAN 收发器初始化后常开。
- 周期广播、App 服务响应、块读数据帧统一进入发送队列。
- CAN 无 ACK、邮箱满、发送超时只记录调试计数，不再调用 `System_ERROR_UserCallback(ERROR_CAN)` 让 App 进入异常。
- 保留原对外接口：`InitCan()`、`App_Can()`、`Can_HDX_Transmit()`、`Can_IsBusy()`、`Can_PrepareSleep()`、`Can_RtcWakeService()`。
- 保留 comm tool/BMS App 服务协议和 CAN-IAP 入口。

## 新结构

`Can_HDX.c` 现在分为四个简单部分：

| 部分 | 职责 |
| --- | --- |
| 初始化 | GPIO、NVIC、CAN1、过滤器配置；CAN1 固定 250 kbit/s |
| 发送队列 | 32 个 `CanTxMsg` 队列，主循环每次只处理一个硬件邮箱 |
| 周期广播 | 1s/5s 周期调用 `CanFeidao_SendNextPending()`，生成业务广播帧 |
| App 服务 | 处理 comm tool 请求：状态、进入 IAP、寄存器读写、块读 |

## 发送策略

- `Can_HDX_Transmit()` 不再直接占用硬件邮箱，而是把帧放入队列。
- 硬件发送统一由 `feidao_can_service_tx()` 管理。
- 一个邮箱发送完成、失败或超时后才发送下一帧。
- `READ_BLOCK` 数据帧按 10ms 间隔分帧入队，避免 UI 实时监控把总线和邮箱打满，同时让 88 个寄存器快照约 0.9s 内返回。
- CAN1 开启 `CAN_ABOM`，bus-off 由硬件自动恢复，软件只记录计数并清理当前队列。

## 过滤器

当前为了调试稳定，过滤器配置为接收所有 CAN 帧，软件层只处理标准帧 App 服务 ID：

```text
(CAN_ADRESS_STD_ID << 7) | 0x60
```

这样可以避免过滤器配置错误导致 comm tool 请求收不到。后续如果总线流量较高，再收紧过滤器。

## 低功耗状态

本次重构不做运行时 CAN 低功耗：

- 不再按“无设备/有设备”动态关闭收发器。
- 不再做发送前上电、发送后关电的状态机。
- `Can_PrepareSleep()` 只在系统准备休眠时清空队列并关闭收发器。
- `Can_RtcWakeService()` 保留接口，简单打开收发器并发送一次 1s 周期帧。

后续如果要重新加低功耗，必须在这个简单发送队列之上增加状态，而不是让业务帧绕过队列直接 `CAN_Transmit()`。

## 测试重点

1. BMS App 启动后，CAN 通讯盒能看到 1s/5s 广播。
2. comm tool `info` 正常。
3. UI `读取BMS状态` 正常。
4. UI `读取BMS信息` 正常，不再触发 BMS App CAN 异常。
5. UI `实时监控` 打开 5 分钟，不触发 BMS App CAN 异常。
6. `使用缓存升级` 能进入 IAP 并升级成功。
7. 不接 CAN 对端时，App 不应因为 ACK 错误进入系统异常。
