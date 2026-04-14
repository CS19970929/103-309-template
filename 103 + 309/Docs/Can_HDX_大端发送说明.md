# Can_HDX 大端发送说明

## 变更目标

将 `Can_HDX.c` 中 `feidao_can_send` 相关的 CAN 发送数据统一改为大端序发送，避免小端平台上通过 `memcpy` 直接打包导致的字节序错误。

## 处理方式

- 新增了 `feidao_put_u16_be`、`feidao_put_u32_be`、`feidao_put_i32_be` 三个辅助函数。
- 所有多字节发送字段改为显式按 `MSB -> LSB` 顺序写入 `data[]`。
- 所有发送缓冲区统一初始化为 `0`，避免未初始化字节被发送出去。

## 影响的发送帧

- 电压/电流帧：`feidao_send_volage_current_1000ms`
- 容量帧：`feidao_send_cap_5000ms`
- SOC 帧：`feidao_send_soc_1000ms`
- SOH 帧：`feidao_send_soh_5000ms`
- 版本帧：`feidao_send_version_5000ms`
- 状态帧：`feidao_send_status_5000ms`

## 版本帧说明

版本帧中 `soft_version` 现在按 16 位大端写入，发送顺序为：

- `data[0]`：协议版本
- `data[1]`：软件版本高字节
- `data[2]`：软件版本低字节

如果接收端此前按单字节解析，需要同步调整。

