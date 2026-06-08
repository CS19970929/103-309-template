# UART / RS485 协议当前实现

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/Sci_Upper.c`, `103 + 309/Project/Source/Sci_Upper.h`, `103 + 309/Project/Source/AppInit.h`, `103 + 309/Project/Source/conf/Project_Config.h`
最后更新时间：2026-05-26
未确认事项：实际量产使用 USART1/USART2/USART3 哪些物理接口仍需结合硬件确认。

## 1. 初始化入口

`AppInit.h` 当前将 `AppInit_InitSci()` 映射为 `InitUSART_CommonUpper()`，因此系统启动阶段会初始化上位机通信串口。

`Sci_Upper.c` 中定义了三套端口运行结构：

- USART1。
- USART2。
- USART3。

当前公共初始化函数会按编译配置初始化对应端口，协议处理复用同一套 Modbus RTU 状态机。

## 2. 串口参数

源码中 `Sci_PortInit()` 使用：

| 参数 | 当前值 |
|---|---|
| Baudrate | `19200` |
| Word length | `8b` |
| Stop bits | `1` |
| Parity | `None` |
| Hardware flow control | `None` |
| Mode | Rx + Tx |

## 3. 中断和收发状态

- RX 使用 `USART_IT_RXNE`。
- 帧结束依赖 `USART_IT_IDLE`。
- TX 使用 `TXE` 连续发送，最后用 `TC` 完成收尾。
- 发生 ORE/NE/FE/PE 时会清错误并重新 armed receiver。
- 发送完成后会调用协议层 `pfOnTxComplete()`，并处理 `u8FlashUpdateE2PROM` 到 `u8FlashUpdateFlag` 的升级触发。

## 4. 低功耗关系

`Project_Config.h` 当前有 UART wakeup 开关：

- `PROJECT_CFG_UART1_WAKEUP_ENABLE = 1`
- `PROJECT_CFG_UART2_WAKEUP_ENABLE = 0`

低功耗模块会把通信忙状态作为阻塞条件之一。后续如果切换通信口或关闭串口唤醒，必须同步验证 STOP 唤醒、上位机在线读写和 IAP 进入。

## 5. 维护规则

1. 修改串口端口、波特率或中断策略，必须回归 Modbus 上位机。
2. 修改 `u8FlashUpdateE2PROM` / `u8FlashUpdateFlag` 触发路径，必须回归 IAP。
3. 若后续抽象客户协议适配层，底层 UART driver 与 Modbus register map 应分离。
