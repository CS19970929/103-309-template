# LX V0.9 7S 协议接入说明

## 目标

本分支固定为 **7 串**，在不删除原 CommonUpper / Modbus RTU 协议的前提下，为 **USART1 和 USART2 同时增加 LX V0.9**。

两个串口使用同一套协议核心和同一套项目数据适配，不复制业务代码。

## 接收路径

```text
USART1 / USART2 RX IRQ
        |
        v
SerialProtocolMux
        |
        +-- 0x00 / 0x01 ------> LegacyModbusProtocol
        |                         |
        |                         +--> 原 Sci_Upper 读写/应答业务
        |
        +-- 0x5A / 0xDD ------> LxPowerProtocol
                                  |
                                  +--> LxPowerProtocolPort
                                           |
                                           +--> BMS / AFE / SOC / Flash / MOS
```

### 设计边界

- `SerialProtocolMux.*`：UART1/2 收发、帧首字节识别、协议分发。
- `LegacyModbusProtocol.*`：保留原 Modbus RTU 收帧状态机，业务仍调用 `Sci_Upper.c` 原函数。
- `LxPowerProtocol.*`：纯协议编解码，不访问 STM32、UART、AFE、Flash、MOS，可独立移植/测试。
- `LxPowerProtocolPort.*`：当前 STM32F103 + SH367309 项目的字段映射和业务适配。
- `CanFeidaoFrames.c`：仅为旧 Keil `.uvprojx` 的构建桥接文件，不承载业务实现。

移植到其它 MCU/AFE 时，优先复用 `LxPowerProtocol.c/.h`，只重写 Port 层和 UART transport。

## 串数

本产品编译期固定：

```c
PROJECT_CFG_SERIES_NUM = 7
```

运行时 `SeriesNum` 和 `OtherElement.u16Sys_SeriesNum` 也被约束为 7。

旧 Modbus 批量写如果覆盖串数字段且写入值不是 7，整帧返回数据非法，避免参数已经落盘后再被静默改回 7。

## UART 波特率

协议分发与 UART 号无关，因此 USART1、USART2 都能识别旧 Modbus 和 LX 帧。

但一个物理 UART 在同一时刻只能工作于一个波特率。本工程保留每端口独立配置：

```c
PROJECT_CFG_SCI1_BAUDRATE
PROJECT_CFG_SCI2_BAUDRATE
```

当前默认保持旧工程的 `19200`，避免仅因增加协议就破坏已存在设备。

LX V0.9 文档规定 `9600` 或 `115200`；如果对端严格按 LX 文档通信，应把对应 UART 配置为 9600 或 115200。当前没有引入自动波特率识别。

## LX V0.9 当前实现

### `5A A5 ... F0`

支持：

- `0x04`：读取 Data1；返回 `CMD=0x01`。
- `0x05`：读取 Data2。
- `0x03`：读取 Data3；返回 `CMD=0x03`。

Data1 固定 7S，`LEN=37 (0x25)`。

文档末尾 `LEN=0x23` 的样例来自旧 V0.8 6S 格式，不作为本 7S 产品实现依据。

### `DD ... 77`

支持：

- `0xAA`：读取 Data4 保护计数。
- `0x01`：清除 Data4 计数来源。
- `0xE1`：软件控制充/放电 MOS。

MOS 命令只能增加/解除“软件禁止”条件；解除时仍检查当前过压、欠压、过流、温度、短路等保护，不通过协议命令绕过安全保护。

## 文档冲突处理

### Data1 长度

V0.9 7S 字段表实际为 37 byte，因此实现为 `0x25`；不发送旧 6S `0x23`。

### Vendor ID

V0.9 正文写 `AA ED`，末尾样例仍为 `AA EE`。当前实现按正文使用 `AA ED`，并集中定义在 `LxPowerProtocol.h`，后续客户确认后只改配置定义。

## Data4 当前限制

当前工程没有 11 个独立的全寿命保护累计计数器。

Data4 暂时从现有 100 条持久化事件日志统计，因此：

- 能掉电保存；
- 能读取/清除；
- 但不是严格的全寿命累计值；
- 原事件日志还有同类事件写入节流策略。

如果客户验收要求“每次保护都累计且长期不丢”，应单独增加带磨损保护的 11 个持久计数器，而不是继续从事件日志反推。

## 审阅重点

1. 原 Modbus `0x03 / 0x06 / 0x10` 的请求长度、CRC、寄存器业务和应答路径没有另起一套实现。
2. USART1 / USART2 只在 `SerialProtocolMux` 中做 transport/dispatch，协议核心不碰硬件。
3. LX core 中 Little Endian、Big Endian、XOR BCC、16-bit two's-complement checksum 分开实现。
4. RX 完整帧之后才进入主循环处理；ISR 不执行 Flash/MOS/AFE 业务。
5. STOP 唤醒后重新初始化 `SerialProtocolMux`，不会退回旧 UART 独占路径。
6. 7S 既有编译期约束，也有运行时/通信写入约束。
