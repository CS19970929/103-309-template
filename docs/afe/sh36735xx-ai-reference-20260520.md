# SH36735XX AFE AI 开发参考

来源文档：`C:/Users/Administrator/xwechat_files/wxid_t7amffgm31vj22_db58/msg/file/2025-08/SH36735XX CV0.2C.pdf`

适用芯片：SH3673510、SH3673514、SH3673517、SH3673520。本文按 PDF `V0.2C` 整理，目标是让后续 AI 或工程师能直接理解 AFE 的寄存器、SPI 通信、采样数据和工程集成约束。

## 1. 芯片能力

- 串数：SH3673510 支持 4-10 串，SH3673514 支持 4-14 串，SH3673517 支持 4-17 串，SH3673520 支持 4-20 串。
- Pack 电压范围：8 V 到 88 V。
- 保护：过充、过放、放电过流 1、放电过流 2、短路、充电过流、充放电高低温、内部高温。
- 采样：13-bit VADC 采电芯、电流、外部温度、内部温度、B+、C+；16-bit CADC 采电流积分用电流。
- 均衡：内置均衡开关，寄存器由 MCU 控制。
- 电源和驱动：LDO1/VCC、LDO2/LDO_O，高侧/低侧充放电 MOS 驱动，预放电 P-MOS 驱动。
- 通信：SPI 从机，Mode 3，MSB first，最高 1 MHz，CRC8。

## 2. 工作模式

| 模式 | 进入方式 | 主要能力 | 退出方式 |
| --- | --- | --- | --- |
| Normal | 默认模式或清 `SCONF1=0x00` | VADC 70 ms，CADC 250 ms，SPI、保护、均衡、WDT 按配置工作 | 写 IDLE/SLEEP/Powerdown/SHIP 条件 |
| IDLE | 无充放电状态且 `SCONF1=0x55` | VADC 280 ms，CADC 4 s，保护和均衡仍可按配置工作 | MCU 清 `SCONF1` 或检测到充放电状态 |
| SLEEP | `SCONF1=0xAA` | SPI/LDO 保持，关闭 CADC/WDT/保护/MOS/均衡/电荷泵，可配置充电器或负载唤醒 | MCU 清 `SCONF1`、充电器唤醒、负载唤醒 |
| Powerdown | `SCONF2.PD_CTL=1` 后立即写 `SCONF1=0x33`，或低压/内部高温/WDT 超时 | 仅保留充电器唤醒，关闭 MOS 和多数模块 | 连接充电器进入 WarmUp |
| SHIP | SHIP 管脚拉低并保持 `tSHIP` | 关闭所有功能，连接充电器无动作 | SHIP 管脚拉高 |

IDLE 模式下，过充、过放、温度、均衡持续、Powerdown 允许、OCD1/OCC 延时通常是 Normal 设置值的 4 倍；看门狗和预放电时间不受 IDLE 倍率影响。

## 3. SPI 通信

### 3.1 电气和时序

- SPI 从机，只支持全双工三线同步传输。
- CPOL=1、CPHA=1，即 SPI Mode 3；SCK 空闲为高电平。
- MSB first。
- SCK 频率不超过 1 MHz。
- CS 低电平选中 AFE。CS 高电平时 SDO 高阻，SDI/SCK 内部上拉。
- 每帧 CS 必须从头到尾保持低电平；CS 拉高超过 `tSPIDIS` 会关闭 SPI 模块。
- CS 拉低期间，如果超过 `tSPIRST` 没有检测到 SCK 下降沿，SPI 模块会复位到初始状态。

### 3.2 CRC8

- 多项式：`x^8 + x^2 + x + 1`，即 `0x07`。
- 初始值：`0x00`。
- 写寄存器 CRC：官方 Demo 与时序图使用 `CRC8(0x01, reg, data)`，不包含长度字节。
- 读寄存器 CRC：AFE 返回 CRC，主机计算 `CRC8(0xFF, 0x02, reg, len, data[0..N-1])` 对比。
- 软件复位 CRC：`CRC8(0x0B, 0xBB, 0xCC)`。

注意：PDF 文字描述里写操作提到“写数据长度”，但写时序图和官方 Demo `SPIApp.c` 都是固定 5 字节写帧，没有长度字节。当前主工程 `sh36735_spi_proto.c` 默认按官方 Demo 的 3 字节写 CRC 实现，这是正确方向。

### 3.3 帧格式

写 RAM 寄存器，地址范围 `0x40-0x59`，一次固定写 1 字节：

| 字节序 | MOSI | MISO |
| --- | --- | --- |
| 0 | `0x01` 写命令 | `0xFF` |
| 1 | 寄存器地址 | `0x01` |
| 2 | 写入数据 | 寄存器地址 |
| 3 | CRC8 | 写入数据 |
| 4 | `0x00` dummy | `0xA5` 成功或 `0xFF` 失败 |

读 RAM 寄存器，地址范围 `0x40-0x99`，长度单位 Byte：

| 字节序 | MOSI | MISO |
| --- | --- | --- |
| 0 | `0x02` 读命令 | `0xFF` |
| 1 | 起始寄存器地址 | `0x02` |
| 2 | 读取长度 `N` | 起始寄存器地址 |
| 3 | `0x00` dummy | `N` |
| 4..3+N | `0x00` dummy | 数据 `data[0..N-1]` |
| 4+N | `0x00` dummy | CRC8 |

软件复位：

| 字节序 | MOSI | MISO |
| --- | --- | --- |
| 0 | `0x0B` | `0xFF` |
| 1 | `0xBB` | `0x0B` |
| 2 | `0xCC` | `0xBB` |
| 3 | CRC8 | `0xCC` |
| 4 | `0x00` dummy | `0xA5` 成功或 `0xFF` 失败 |

## 4. 采样和换算

- 电芯电压：`Vcell_mV = CELLn * 5 / 32`。工程里用 `((raw * 5) >> 5)`。
- B+ 和 C+：同样先按 `raw * 5 / 32` 得到等效采样电压，再乘内部比例系数；当前工程乘 `25`。
- 外部温度：`TEMPn` 是外部热敏电阻分压 ADC 码。当前工程按 10 k 上拉计算 `R_ntc(kΩ*100) = 1000 * TEMP / (32768 - TEMP)`，再查 NTC 表。
- VADC 电流：PDF 给出 `Current = 100 * CUR / (29127 * RSENSE)`，其中 `RSENSE` 单位为欧姆。
- CADC 电流：`CADCD.15` 为方向位，1 表示放电，0 表示充电。当前工程目前读取 CADC 寄存器作为主电流来源。
- FLAG2 的 `VADC_FLG`、`CADC_FLG` 在读取 FLAG2 后会硬件自动清零；读取状态块时必须先锁存这两个 bit。

## 5. 保护和清除规则

- 过充、过放、OCD1、OCD2、短路、充电过流进入保护后会关闭相应 MOS，并置位 FLAG1 对应 bit。
- 充电低温、充电高温、放电低温、放电高温进入保护后会关闭相应 MOS，并置位 FLAG2 对应 bit。
- 清保护标志的前提：先把 `SCONF2.LTCLR` 置 1，再向 FLAG1/FLAG2 对应 bit 写 0。
- `FLAG1[7:0]` 均为读/写 0 清除。
- `FLAG2[7:2]` 为读/写 0 清除；`FLAG2[1:0]` 是 VADC/CADC 转换完成标志，只读且读后自动清零。
- `FLAG3.OWD_FLG` 读后自动清零。

## 6. 完整寄存器速查

| 地址 | 名称 | 访问 | 含义 |
| --- | --- | --- | --- |
| `0x40` | `SCONF1` | R/W | 模式控制。`0x00` Normal，`0x55` IDLE，`0xAA` SLEEP，`0x33` Powerdown 配合 `PD_CTL` |
| `0x41` | `SCONF2` | R/W | `LTCLR`、Powerdown、PUMP、预放电控制、DSG/CHG MOS 控制 |
| `0x42` | `SCONF3` | R/W | 充电器唤醒、负载唤醒、C+/负载状态检测、断线检测控制 |
| `0x43` | `SCONF4` | R/W | `PDSGT[2:0]` 预放电时间，`CN[4:0]` 串数 |
| `0x44` | `SCONF5` | R/W | MOS 强制开启、OCC、CADC、WDT 使能和 WDT 时间 |
| `0x45` | `SCONF6` | R/W | TS4/TS3/TS2/TS1、SC、OCD、UV、OV 保护使能 |
| `0x46` | `SCONF7` | R/W | IDLE CADC 更新周期、负载检测放电电流、充放电状态检测阈值 |
| `0x47` | `OWV/ALARMH` | R/W | 断线检测电压阈值高位和 LOADON/LOADOFF/VADC/CADC ALARM 使能 |
| `0x48` | `ALARML` | R/W | WK/WDT/OWD/TEMP/OCC/OCD/UV/OV ALARM 使能 |
| `0x49` | `OVT/OVH` | R/W | `OVT[2:0]` 和 `OV[9:8]` |
| `0x4A` | `OVL` | R/W | `OV[7:0]` |
| `0x4B` | `UVT/UVH` | R/W | `UVT[2:0]` 和 `UV[9:8]` |
| `0x4C` | `UVL` | R/W | `UV[7:0]` |
| `0x4D` | `OCD1V/OCD1T` | R/W | OCD1 延时和阈值 |
| `0x4E` | `OCD2V/OCD2T` | R/W | OCD2 延时和阈值 |
| `0x4F` | `SCV/SCT` | R/W | 短路阈值倍率和短路延时 |
| `0x50` | `OCCV/OCCT` | R/W | 充电过流阈值和延时 |
| `0x51` | `OTC` | R/W | 充电高温阈值 |
| `0x52` | `OTD` | R/W | 放电高温阈值 |
| `0x53` | `UTC` | R/W | 充电低温阈值 |
| `0x54` | `UTD` | R/W | 放电低温阈值 |
| `0x55` | `BALANCEH` | R/W | `CB20..CB17` |
| `0x56` | `BALANCEM` | R/W | `CB16..CB9` |
| `0x57` | `BALANCEL` | R/W | `CB8..CB1` |
| `0x58` | `FLAG1` | R/W0 | RST1、WK、OCC、SC、OCD2、OCD1、UV、OV 标志 |
| `0x59` | `FLAG2` | R/W0/RO | OTD、UTD、OTC、UTC、RST2、WDT、VADC、CADC 标志 |
| `0x5A` | `FLAG3` | RO | 断线检测奇偶指示和完成标志 |
| `0x5B` | `BSTATUS1` | RO | E2P、HDSG、HCHG、PDSG、DSG、CHG FET 状态 |
| `0x5C` | `BSTATUS2` | RO | CHGING、DSGING、SLEEP、IDLE、BAL、LOADON、LOADOFF 状态 |
| `0x5D` | `TEMP1H` | RO | TS1 高字节 |
| `0x5E` | `TEMP1L` | RO | TS1 低字节 |
| `0x5F` | `TEMP2H` | RO | TS2 高字节 |
| `0x60` | `TEMP2L` | RO | TS2 低字节 |
| `0x61` | `TEMP3H` | RO | TS3 高字节 |
| `0x62` | `TEMP3L` | RO | TS3 低字节 |
| `0x63` | `TEMP4H` | RO | TS4 高字节 |
| `0x64` | `TEMP4L` | RO | TS4 低字节 |
| `0x65` | `TEMPIH` | RO | 内部温度高字节 |
| `0x66` | `TEMPIL` | RO | 内部温度低字节 |
| `0x67` | `CURH` | RO | VADC 电流高字节，`CUR.15` 为方向 |
| `0x68` | `CURL` | RO | VADC 电流低字节 |
| `0x69` | `CELL1H` | RO | Cell1 高字节 |
| `0x6A` | `CELL1L` | RO | Cell1 低字节 |
| `0x6B` | `CELL2H` | RO | Cell2 高字节 |
| `0x6C` | `CELL2L` | RO | Cell2 低字节 |
| `0x6D` | `CELL3H` | RO | Cell3 高字节 |
| `0x6E` | `CELL3L` | RO | Cell3 低字节 |
| `0x6F` | `CELL4H` | RO | Cell4 高字节 |
| `0x70` | `CELL4L` | RO | Cell4 低字节 |
| `0x71` | `CELL5H` | RO | Cell5 高字节 |
| `0x72` | `CELL5L` | RO | Cell5 低字节 |
| `0x73` | `CELL6H` | RO | Cell6 高字节 |
| `0x74` | `CELL6L` | RO | Cell6 低字节 |
| `0x75` | `CELL7H` | RO | Cell7 高字节 |
| `0x76` | `CELL7L` | RO | Cell7 低字节 |
| `0x77` | `CELL8H` | RO | Cell8 高字节 |
| `0x78` | `CELL8L` | RO | Cell8 低字节 |
| `0x79` | `CELL9H` | RO | Cell9 高字节 |
| `0x7A` | `CELL9L` | RO | Cell9 低字节 |
| `0x7B` | `CELL10H` | RO | Cell10 高字节 |
| `0x7C` | `CELL10L` | RO | Cell10 低字节 |
| `0x7D` | `CELL11H` | RO | Cell11 高字节 |
| `0x7E` | `CELL11L` | RO | Cell11 低字节 |
| `0x7F` | `CELL12H` | RO | Cell12 高字节 |
| `0x80` | `CELL12L` | RO | Cell12 低字节 |
| `0x81` | `CELL13H` | RO | Cell13 高字节 |
| `0x82` | `CELL13L` | RO | Cell13 低字节 |
| `0x83` | `CELL14H` | RO | Cell14 高字节 |
| `0x84` | `CELL14L` | RO | Cell14 低字节 |
| `0x85` | `CELL15H` | RO | Cell15 高字节 |
| `0x86` | `CELL15L` | RO | Cell15 低字节 |
| `0x87` | `CELL16H` | RO | Cell16 高字节 |
| `0x88` | `CELL16L` | RO | Cell16 低字节 |
| `0x89` | `CELL17H` | RO | Cell17 高字节 |
| `0x8A` | `CELL17L` | RO | Cell17 低字节 |
| `0x8B` | `CELL18H` | RO | Cell18 高字节 |
| `0x8C` | `CELL18L` | RO | Cell18 低字节 |
| `0x8D` | `CELL19H` | RO | Cell19 高字节 |
| `0x8E` | `CELL19L` | RO | Cell19 低字节 |
| `0x8F` | `CELL20H` | RO | Cell20 高字节 |
| `0x90` | `CELL20L` | RO | Cell20 低字节 |
| `0x91` | `CADCDH` | RO | CADC 电流高字节，`CADCD.15` 为方向 |
| `0x92` | `CADCDL` | RO | CADC 电流低字节 |
| `0x93` | `VTOPH` | RO | B+ 电压高字节 |
| `0x94` | `VTOPL` | RO | B+ 电压低字节 |
| `0x95` | `VCHGRH` | RO | C+ / CHGD 电压高字节 |
| `0x96` | `VCHGRL` | RO | C+ / CHGD 电压低字节 |
| `0x97` | `OWDH` | RO | 断线检测 `OWD20..OWD17` |
| `0x98` | `OWDM` | RO | 断线检测 `OWD16..OWD9` |
| `0x99` | `OWDL` | RO | 断线检测 `OWD8..OWD1` |

## 7. 关键配置换算

| 项目 | 寄存器 | 换算 |
| --- | --- | --- |
| 过充阈值 | `0x49[1:0] + 0x4A` | `VOV = code * 5 mV`，默认 `0x348 = 4200 mV` |
| 过充延时 | `0x49[6:4]` | `000=140 ms, 001=280 ms, 010=490 ms, 011=0.98 s, 100=2.03 s, 101=3.01 s, 110=4.97 s, 111=10.01 s` |
| 过放阈值 | `0x4B[1:0] + 0x4C` | `VUV = code * 5 mV`，默认 `0x21C = 2700 mV` |
| 过放延时 | `0x4B[6:4]` | `000=490 ms, 001=770 ms, 010=0.98 s, 011=1.47 s, 100=2.03 s, 101=3.01 s, 110=4.97 s, 111=10.01 s` |
| OCD1 阈值 | `0x4D[3:0]` | `VOCD1 = code * 5 mV + 5 mV`，默认 `0x09 = 50 mV` |
| OCD1 延时 | `0x4D[6:4]` | 同 OVT 延时表 |
| OCD2 阈值 | `0x4E[3:0]` | `VOCD2 = code * 10 mV + 10 mV`，默认 `0x09 = 100 mV` |
| OCD2 延时 | `0x4E[7:4]` | `tOCD2 = code * 25 ms + 25 ms`，默认 `0x03 = 100 ms` |
| SC 阈值 | `0x4F[5:4]` | `00=2*VOCD2, 01=3*VOCD2, 10=4*VOCD2, 11=6*VOCD2` |
| SC 延时 | `0x4F[3:0]` | `0000=0 us, 0001=32 us, 0010=64 us, 0011=96 us, 0100=128 us, 0101=192 us, 0110=224 us, 0111=256 us, 1000=288 us, 1001=320 us, 1010=384 us, 1011=448 us, 1100=480 us, 1101=512 us, 1110=544 us, 1111=576 us` |
| OCC 阈值 | `0x50[4:0]` | `VOCC = code * 1.375 mV + 1.375 mV`，默认 `0x0F ≈ 22 mV` |
| OCC 延时 | `0x50[7:5]` | 同 OVT 延时表 |
| WDT 时间 | `0x44[1:0]` | `00=32.34 s, 01=15.68 s, 10=7.84 s, 11=3.92 s` |
| IDLE CADC 更新 | `0x46[5:4]` | `00=4 s, 01=32 s, 10=64 s, 11=256 s` |
| 预放时间 | `0x43[7:5]` | `000=0.21 s, 001=0.28 s, 010=0.42 s, 011=0.49 s, 100=0.63 s, 101=0.98 s, 110=2.03 s, 111=3.01 s` |

## 8. 当前项目应遵守的开发约束

- 以 `103 + 309/Project/Source/SH36735_reg.h` 作为 SH36735XX 地址真源，不使用旧 SH367309/I2C 寄存器地址。
- SPI 协议只通过 `sh36735_write_reg_u8()`、`sh36735_read_regs()`、`sh36735_sw_reset()` 访问。
- 写配置寄存器后要读回校验；涉及只读、自清、写 0 清标志的寄存器要用掩码校验。
- 读取 `FLAG2` 后必须锁存 `VADC_FLG/CADC_FLG`，否则采样完成标志会丢失。
- 串数进入工程后必须限制在 4 到 20 串，并同步 `SeriesNum`、`OtherElement.u16Sys_SeriesNum` 和 `SCONF4.CN`。
- 进入 SLEEP 前必须先关闭均衡，并确认 SPI 写 `SCONF1=0xAA` 成功。
- 任何重新启用 `MTPRead/MTPWrite`、`AFE_Reset`、`InitAFE1`、`SH367309_UpdataAfeConfig` 的改动都需要先确认它们已从旧 I2C 语义迁移到 SH36735XX SPI 语义。
