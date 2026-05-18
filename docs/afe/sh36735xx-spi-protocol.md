# SH36735XX SPI 通信整理

## 1. 芯片要求

SH36735XX 的 SPI 要求来自 `SH36735XX CV0.2C.pdf` 第 25~27 页和第 51 页：

- AFE 是 SPI 从机，MCU 是主机。
- 最大 SCK 频率 1 MHz。
- 固定 SPI Mode 3：`CPOL=1`、`CPHA=1`。
- MSB first。
- `CS` 低有效，通信开始前拉低，帧结束后拉高。
- `CS` 高电平时，AFE 忽略 SCK，SDO 高阻。
- `SCK` 高/低电平时间最小 500 ns。
- `CS` 拉高超过 `tSPIDIS` 后 SPI 模块关闭。
- `CS` 拉低期间 SCK 长时间无下降沿，超过约 1 s 后 SPI 模块复位。

## 2. 写寄存器帧

写 RAM 寄存器地址范围是 `0x40..0x59`，固定写 1 字节。

```text
MOSI: 0x01, RegAddr, Data, CRC8, 0x00
MISO: 0xFF, 0x01, RegAddr, Data, ACK
```

成功 ACK 为 `0xA5`，失败为 `0xFF`。最后一个 `0x00` 不是无意义代码，它用于继续产生 8 个 SCK，让 MCU 在 MISO 上读到 ACK。

当前主工程实现：

- `sh36735_write_reg_u8_once()` 发送 5 字节。
- 校验 `0xFF`、命令回显、地址回显、数据回显和 `0xA5`。
- 对外 `sh36735_write_reg_u8()` 重试 5 次，每次失败延时约 1 ms。

与官方例程 `SH_AFE_SPI_Write()` 一致。

## 3. 读寄存器帧

读 RAM 寄存器地址范围是 `0x40..0x99`，长度字段不包含最后 CRC。

```text
MOSI: 0x02, RegAddr, Len, 0x00, Len 个 0x00, 0x00
MISO: 0xFF, 0x02, RegAddr, Len, Len 个 Data, CRC8
```

当前主工程实现：

- `sh36735_read_regs_once()` 读取完整帧。
- CRC 从 `0xFF` 开始计算，覆盖 `0xFF/cmd/reg/len/data`。
- 只有帧头、回显和 CRC 全部通过后才拷贝数据到调用方 buffer。
- 对外 `sh36735_read_regs()` 重试 5 次。

与官方例程 `SH_AFE_SPI_Read()` 一致。

## 4. 软件复位帧

```text
MOSI: 0x0B, 0xBB, 0xCC, CRC8, 0x00
MISO: 0xFF, 0x0B, 0xBB, 0xCC, ACK
```

当前主工程 `sh36735_sw_reset_once()` 已发送最后一个 `0x00` 读取 ACK。注意软件复位会复位 RAM 寄存器、VADC、CADC 和 SPI 模块，因此复位后必须重新写 AFE 配置。

## 5. CRC8

芯片文档给出的 CRC8：

- polynomial：`0x07`
- init：`0x00`

官方例程写操作按 3 字节 `cmd, reg, data` 计算 CRC。PDF 文字描述里提到“写数据长度”，但写时序图和官方例程都没有长度字节。主工程当前默认 `SH_WRITE_CRC_INCLUDE_LEN=0`，应保持这个行为，除非后续实测证明芯片批次有差异。

## 6. 主工程当前 SPI 结论

当前 SPI 底层不再是主要疑点：

- Mode 3 正确。
- 软件 SPI 约 500 kHz，满足 1 MHz 上限。
- 写、读、复位帧格式与官方例程一致。
- 已有回显、ACK、CRC 和重试。
- Keil 工程当前只编译软件 SPI，硬件 SPI 文件被排除，不存在双实现重复链接。

后续需要补的不是帧格式，而是接口防护：

- 写地址限制到 `0x40..0x59`。
- 读地址限制到 `0x40..0x99`。
- 检查 `reg + len - 1` 不超过 `0x99`。
- 限制单次最大读取长度，避免误调用造成大栈占用。
- 记录最后失败的命令、地址、长度、CRC 状态，便于 ST-Link 调试。
