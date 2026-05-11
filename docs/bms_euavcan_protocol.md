# BMS E-UAVCAN 协议梳理

来源：`bms协议文档.pdf`

本文只整理 PDF 中明确给出的协议内容，并补充后续在本工程实现时需要注意的映射点。代码实现前需要先确认文末的待确认项。

## 1. 协议总览

- 协议对象：智能电池 BMS 与外部设备通讯。
- 底层总线：CANBus 2.0B。
- 帧类型：29 bit 扩展数据帧。
- 波特率：1000 Kbps。
- 上报方式：BMS 主动上报。
- 已定义消息：`info`。
- `info` 消息 Data Type ID：`0x1092`。
- `info` 消息上报频率：4 Hz。
- 所有业务数据小端传输。
- 单个 CAN 数据域最多 8 字节，其中最后 1 字节为 Tail byte，最多承载 7 字节有效业务数据。

## 2. 29 bit CAN ID

PDF 中给出的 ID 字段分配：

| 字段 | 位数 | 取值 | 说明 |
| --- | ---: | --- | --- |
| Priority | 5 | 0..31 | 表格说明默认填最高优先级 `0` |
| Message type ID | 16 | `0x1092` | `info` 消息 |
| Service or message | 1 | `0` | `0x1092` 该位为 `0` |
| Source node ID | 7 | 1..127 | BMS 默认 Source node ID 为 `0x16` |

按字段位定义组装：

```text
CAN ID = (Priority << 24) | (MessageTypeID << 8) | (ServiceBit << 7) | SourceNodeID
```

如果 Priority 按表格默认 `0`，则：

```text
CAN ID = 0x00109216
```

PDF 页 1 红字同时写了“BMS 该节点的帧 id 需要为 1109216，扩展帧”。该描述和字段表存在歧义：

- 若按十六进制字符串理解，可能是 `0x1109216`，对应 Priority = `1`、Message type ID = `0x1092`、Source node ID = `0x16`。
- 若按十进制数理解，`1109216` 等于 `0x10ECE0`，无法和字段表中的 `0x1092` / `0x16` 对齐。

后续实现前建议优先和对端确认最终期望的扩展帧 ID。

## 3. CAN 数据域和 Tail byte

每个 CAN 帧数据域最多 8 字节：

| 字段 | 长度 | 说明 |
| --- | ---: | --- |
| Transfer payload | 最多 7 字节 | 实际传输的业务负载 |
| Tail byte | 1 字节 | 传输层控制信息 |

Tail byte 位定义：

| 位 | 字段 | 长度 | 说明 |
| ---: | --- | ---: | --- |
| 7 | Start of transfer | 1 bit | 多帧传输首帧为 `1`，其他帧为 `0` |
| 6 | End of transfer | 1 bit | 多帧传输末帧为 `1`，其他帧为 `0` |
| 5 | Toggle bit | 1 bit | 多帧传输中逐帧翻转；按 UAVCAN 习惯首帧为 `0`，后续 `1/0/1...` |
| 4..0 | Transfer ID | 5 bit | 当前 transfer ID，范围 `0..31`，每次完整传输后递增并回卷 |

Tail byte 组包公式：

```text
Tail = (Start << 7) | (End << 6) | (Toggle << 5) | (TransferID & 0x1F)
```

## 4. 多帧传输格式

PDF 只给出了 multi-frame transfer 图示。对本协议的 `info` 消息，业务负载为 64 字节，必须使用多帧。

多帧格式：

| 帧 | Data 字节布局 | Tail 标志 |
| --- | --- | --- |
| 首帧 | Byte0 = CRC Low，Byte1 = CRC High，Byte2..6 = 前 5 字节 payload，Byte7 = Tail | Start = 1，End = 0，Toggle = 0 |
| 中间帧 | Byte0..6 = 后续 7 字节 payload，Byte7 = Tail | Start = 0，End = 0，Toggle 逐帧翻转 |
| 末帧 | Byte0..N-1 = 最后 N 字节 payload，ByteN = Tail，N 属于 1..7 | Start = 0，End = 1，Toggle 逐帧翻转 |

`info` 负载长度为 64 字节时：

- 首帧承载 CRC 2 字节 + payload 5 字节。
- 剩余 payload：59 字节。
- 中间完整帧：8 帧，每帧 7 字节，共 56 字节。
- 末帧：3 字节 payload + 1 字节 Tail。
- 总帧数：10 帧。

## 5. CRC

PDF 附录给出 CRC-CCITT 算法：

- 初始值：`0xFFFF`
- 多项式：`0x1021`
- 输入：业务 payload 字节数组
- 输出：16 bit CRC
- 首帧中按小端发送：CRC Low 在 Byte0，CRC High 在 Byte1

整理后的伪代码：

```c
uint16_t crc = 0xFFFF;
for each byte in payload:
    crc ^= (uint16_t)byte << 8;
    repeat 8 times:
        if (crc & 0x8000):
            crc = (crc << 1) ^ 0x1021;
        else:
            crc = crc << 1;
```

## 6. `info(0x1092)` 消息负载

PDF 写明 `info` 数据域为 12S，字段如下。所有多字节字段小端。

| 偏移 | 字段 | 类型 | 长度 | 单位/说明 | 当前工程建议数据源 |
| ---: | --- | --- | ---: | --- | --- |
| 0 | 厂商编号 | short | 2 | PDF 备注为 `********` | 需确认常量 |
| 2 | 电池型号编码 | short | 2 | 未给默认值 | 需确认常量 |
| 4 | 电池电压 | unsigned short | 2 | mV | `g_stCellInfoReport.u16VCellTotle` 当前注释为 `V * 100`，实现时需换算成 mV |
| 6 | 充放电电流 | short | 2 | 10 mA；正数充电，负数放电 | `u16Ichg` / `u16IDischg`，当前单位 A * 10，恰好等于 10 mA |
| 8 | 电池温度 | short | 2 | 1 degC | 可用最低/最高/平均温度，需确认；工程温度为 `(degC + 40) * 10` |
| 10 | 电量百分比 | unsigned short | 2 | % | `g_stCellInfoReport.SocElement.u16Soc` |
| 12 | 循环计数 | unsigned short | 2 | 次数 | `g_stCellInfoReport.SocElement.u16Cycle_times` |
| 14 | 健康状况 | short | 2 | %，按电池化学特性曲线分析生成 | `g_stCellInfoReport.SocElement.u16Soh` |
| 16 | 电池 1 电压 | unsigned short | 2 | mV | `g_stCellInfoReport.u16VCell[0]` |
| 18 | 电池 2 电压 | unsigned short | 2 | mV | `g_stCellInfoReport.u16VCell[1]` |
| 20 | 电池 3 电压 | unsigned short | 2 | mV | `g_stCellInfoReport.u16VCell[2]` |
| 22 | 电池 4 电压 | unsigned short | 2 | mV | `g_stCellInfoReport.u16VCell[3]` |
| 24 | 电池 5 电压 | unsigned short | 2 | mV | `g_stCellInfoReport.u16VCell[4]` |
| 26 | 电池 6 电压 | unsigned short | 2 | mV | `g_stCellInfoReport.u16VCell[5]` |
| 28 | 电池 7 电压 | unsigned short | 2 | mV | `g_stCellInfoReport.u16VCell[6]` |
| 30 | 电池 8 电压 | unsigned short | 2 | mV | `g_stCellInfoReport.u16VCell[7]` |
| 32 | 电池 9 电压 | unsigned short | 2 | mV | `g_stCellInfoReport.u16VCell[8]` |
| 34 | 电池 10 电压 | unsigned short | 2 | mV | `g_stCellInfoReport.u16VCell[9]` |
| 36 | 电池 11 电压 | unsigned short | 2 | mV | `g_stCellInfoReport.u16VCell[10]` |
| 38 | 电池 12 电压 | unsigned short | 2 | mV | `g_stCellInfoReport.u16VCell[11]` |
| 40 | 电池设计容量 | unsigned short | 2 | mAh，PDF 备注 `0` | 当前 SOC 容量为 Ah * 100，实现时需换算成 mAh 或按 PDF 置 0 |
| 42 | 电池剩余容量 | unsigned short | 2 | mAh，PDF 备注 `0` | 当前 SOC 容量为 Ah * 100，实现时需换算成 mAh 或按 PDF 置 0 |
| 44 | 错误信息 | uint32 | 4 | 每 bit 表示一种错误状态 | 见下一节映射 |
| 48 | 电池序列号 | char[16] | 16 | ASCII | `ProductionInfor.BMS_SerialNumber`，不足补 `0x00` |

总长度：64 字节。

## 7. 错误信息 bit 映射

PDF 定义：

| Bit | 错误 | 说明 |
| ---: | --- | --- |
| 0 | 电池温度过低 | `1` 表示错误发生，`0` 表示无错误 |
| 1 | 电池过温 | 同上 |
| 2 | 充电过流 | 同上 |
| 3 | 放电过流 | 同上 |
| 4 | 总电压欠压 | 同上 |
| 5 | 总电压过压 | 同上 |
| 6 | 单节压差过大 | 同上 |
| 7 | 单节电压过压 | 同上 |
| 8 | 单节电压欠压 | 同上 |
| 9 | 充电短路 | 同上 |
| 10 | 放电短路 | 同上 |
| 11 | 电池剩余容量过低 | 同上 |
| 12 | 非原装充电器充电 | 同上 |
| 13..31 | 保留 | 填 0 |

当前工程可初步映射：

| PDF Bit | 当前工程候选状态 |
| ---: | --- |
| 0 | `b1CellChgUtp` 或 `b1CellDischgUtp` |
| 1 | `b1CellChgOtp` 或 `b1CellDischgOtp` 或 `b1TmosOtp` |
| 2 | `b1IchgOcp` |
| 3 | `b1IdischgOcp` |
| 4 | `b1BatUvp` |
| 5 | `b1BatOvp` |
| 6 | `b1VcellDeltaBig` |
| 7 | `b1CellOvp` |
| 8 | `b1CellUvp` |
| 9 | 当前未见独立充电短路标志，需确认 |
| 10 | 当前可参考 `System_ErrFlag.u8ErrFlag_CBC_DSG`，需确认是否等价放电短路 |
| 11 | `b1SocLow` |
| 12 | 当前未见非原装充电器识别标志，默认 0 |

## 8. 当前工程实现落点

项目中已有两套相关通信代码：

- `Project/Source/Can_HDX.c/.h`：标准帧请求/响应式 CAN 上报，`App_Can()` 当前在 `main.c` 中被注释。
- `Project/Source/main.c`：已有 libcanard/DroneCAN `BatteryInfo` 上报逻辑，当前 `test_dronecan()` 以 1 Hz 调用 `send_BatteryInfo()`，但它使用的是标准 DroneCAN `uavcan.equipment.power.BatteryInfo`，不是 PDF 中的 64 字节 `info(0x1092)` 负载。

后续实现建议：

1. 新增独立模块，例如 `Bms_EUavcan.c/.h`，避免继续把自定义协议塞进 `main.c`。
2. 保留现有 CAN 初始化和底层发送能力，但需要按 PDF 确认 CAN 波特率改为 1000 Kbps。
3. 停用或替换当前 `send_BatteryInfo()` / `test_dronecan()` 的 DroneCAN 标准电池信息上报，避免总线上同时出现两套不同的 `BatteryInfo/info` 语义。
4. 用状态机发送 10 帧 multi-frame transfer，避免一次性连续塞满 3 个 CAN TX mailbox 后丢帧。
5. 上报周期用 10 ms 系统节拍累计到 250 ms，满足 4 Hz。

## 9. 本次实现决策

已在当前工程实现独立模块：

- 新增 `Project/Source/Bms_EUavcan.c`
- 新增 `Project/Source/Bms_EUavcan.h`
- 主循环调用 `App_BmsEUavcan()`
- 初始化阶段调用 `InitBmsEUavcan()`
- Keil 工程已加入 `Bms_EUavcan.c`

实现策略：

- 使用标准外设库 `CAN_Transmit()` 直接发送 29 bit 扩展帧。
- CAN 初始化调整为 1 Mbps。当前系统时钟配置为 HSE 8 MHz 直驱，CAN 位时序为 Prescaler = 1、BS1 = 6 tq、BS2 = 1 tq。
- 默认 CAN ID 使用 `0x01109216`，即按 PDF 红字“1109216”作为十六进制帧号理解；如对端要求按字段表最高优先级 `0`，只需将 `BMS_EUAVCAN_PRIORITY` 从 `1` 改为 `0`。
- `info` 负载固定 64 字节，首帧带 CRC-CCITT，小端发送 CRC Low/High。
- 每次完整 transfer 后 `Transfer ID` 加 1 并按 5 bit 回卷。
- 每 250 ms 开始一次新 transfer；每 10 ms 发送 1 帧，完整 10 帧约 100 ms 发完。
- 厂商编号、电池型号编码暂填 `0`，分别由 `BMS_EUAVCAN_MANUFACTURER_ID` 和 `BMS_EUAVCAN_BATTERY_MODEL_ID` 控制。
- 设计容量、剩余容量按 PDF 备注默认填 `0`；后续确认需要 mAh 后，可将 `BMS_EUAVCAN_REPORT_CAPACITY_MAH` 置 `1`。
- 电池温度字段当前取 `u16TempMax` 换算为摄氏度整数，避免过温场景被低温通道掩盖。
- 错误位按当前工程 `unMdlFault_Third` 和 `System_ErrFlag.u8ErrFlag_CBC_DSG` 映射，未发现来源的 Bit9/Bit12 暂填 0。

## 10. 验证记录

- 2026-05-11：使用 `C:\Keil_v5\UV4\UV4.exe` 构建 `CommomSH367309_16series_103RCT6_C.uvprojx` 的 `Target 1`。
- 编译器：ARMCC V5.06 update 7 build 960。
- 结果：0 Error(s), 0 Warning(s)。
- 固件大小：Code = 50252，RO-data = 2932，RW-data = 1544，ZI-data = 6152。
- 产物：`Project/Users/Objects/CommomSH367309_16series_103RCT6_C.axf`、`Project/Users/Objects/CommomSH367309_16series_103RCT6_C.bin`。

## 11. 待确认项

实现前至少确认以下内容：

1. 最终扩展帧 ID：当前实现使用 `0x01109216`；若对端按字段表要求最高优先级 `0`，需要改为 `0x00109216`。
2. 厂商编号和电池型号编码的实际取值。
3. 电池温度字段使用最高温、最低温、平均温，还是指定某一路温度。
4. 设计容量/剩余容量字段是否按 PDF 备注固定填 `0`，还是按工程 SOC 容量换算为 mAh。
5. 充电短路、放电短路、非原装充电器三个错误位在当前硬件/软件中的准确来源。
6. 是否需要从 Keil 工程中移除未使用的 DroneCAN/libcanard 源文件，进一步降低固件体积。
