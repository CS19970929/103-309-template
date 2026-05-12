# CAN 运行时状态收口说明

## 背景

`Can_HDX.c` 原先使用多组 `s_u8FeidaoCan*`、`s_u16FeidaoCan*` 和 `s_u32FeidaoCan*` 文件内静态变量维护飞道 CAN 的低功耗发送、周期调度、ACK 判定和上位机 IAP 复位状态。变量数量较多，后续排查时很难判断哪些状态属于同一个运行时上下文。

本次重构将这些文件内状态收口到 `FeidaoCanRuntime`：

- `s_feidao_can_runtime.power_state`
- `s_feidao_can_runtime.tx_mailbox`
- `s_feidao_can_runtime.pending_mask`
- `s_feidao_can_runtime.power_tick`
- `s_feidao_can_runtime.tx_tick`
- `s_feidao_can_runtime.logical_tick`
- `s_feidao_can_runtime.last_hw_tick`
- `s_feidao_can_runtime.last_1000ms_tick`
- `s_feidao_can_runtime.last_5000ms_tick`
- `s_feidao_can_runtime.hw_tick_valid`
- `s_feidao_can_runtime.schedule_init`
- `s_feidao_can_runtime.bus_active`
- `s_feidao_can_runtime.no_ack_cnt`
- `s_feidao_can_runtime.probe_active`
- `s_feidao_can_runtime.rtc_service_active`
- `s_feidao_can_runtime.tx_cycle_acked`
- `s_feidao_can_runtime.tx_cycle_no_ack_recorded`
- `s_feidao_can_runtime.host_iap_reset_pending`
- `s_feidao_can_runtime.host_iap_reset_delay_ticks`

## 兼容策略

为降低风险，本次没有改动 `Can_HDX.c` 中既有函数逻辑，也没有改动对外接口、CAN 报文格式、Flash 参数结构或 IAP 地址规则。

旧变量名通过文件内宏映射到 `s_feidao_can_runtime` 字段，例如：

```c
#define s_u8FeidaoCanPowerState (s_feidao_can_runtime.power_state)
```

这样可以保持原调用点和表达式行为不变，同时把运行时状态集中到一个结构体里，便于后续继续分阶段清理。

## 后续建议

后续如果继续重构，应按以下顺序推进：

1. 先为 `FeidaoCanRuntime` 增加只在 `Can_HDX.c` 内使用的 reset/init helper。
2. 再把宏调用点分批替换为 `s_feidao_can_runtime.xxx`。
3. 最后删除兼容宏。

每一步都应保持 CAN 上位机 IAP 命令、RTC 低功耗 CAN 服务和周期报文调度行为不变。
