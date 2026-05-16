# 客户串口协议

## 相关文件

- [Uart_Client.c](../../Code/Source/Uart_Client.c)
- [Uart_Client.h](../../Code/Include/Uart_Client.h)

## 模块状态

该模块是客户定制串口协议，源码参与工程，但默认未启用：

- `_CLIENT_SCI1` 未定义。
- `_CLIENT_SCI2` 未定义。
- 默认运行路径使用 `Sci_Upper`。

## 硬件资源

`Uart_Client` 复用 USART1/USART2 引脚：

| 通道 | GPIO | 默认波特率 |
| --- | --- | --- |
| USART1 | PA9 TX / PA10 RX | 9600 |
| USART2 | PA2 TX / PA3 RX | 9600 |

与 `Sci_Upper` 共用硬件资源，不能在同一 USART 上同时启用两个协议。

## 协议特征

| 项目 | 值 |
| --- | --- |
| 帧头 | `0x5A` |
| 帧尾 | `0xFB` |
| 命令 | `0xA1`、`0xA2` |

协议内容与客户需求绑定，主要用于定制状态上传或控制。

## 维护建议

- 启用前必须在 `main.h` 中关闭对应 USART 的 `_COMMOM_UPPER_SCIx`，避免中断处理冲突。
- 若客户协议与公共协议都需要保留，建议按通道拆分，而不是在同一串口动态混跑。
- 客户协议新增字段时，应同步维护上位机解析文档和版本号。
