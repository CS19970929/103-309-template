# SH36735XX AFE 四源交叉对比

对比日期：2026-05-20

对比对象：

- PDF 原始手册：`C:/Users/Administrator/xwechat_files/wxid_t7amffgm31vj22_db58/msg/file/2025-08/SH36735XX CV0.2C.pdf`
- 已提取文档：`docs/afe/sh36735xx-ai-reference-20260520.md`
- 官方例程：`SH3673520+STM32F072CBT6 DemoCode V1.2_20241227`
- 主项目工程：`103 + 309/Project/Source`

本文目的不是重复完整寄存器表，而是确认四个来源是否互相一致，并明确后续开发应以哪个结论为准。

## 1. 总体结论

| 对比项 | 结论 |
| --- | --- |
| PDF vs 已提取文档 | 已提取文档覆盖了工作模式、SPI 帧、CRC、寄存器 `0x40-0x99`、采样换算和保护清除规则，整体与 PDF 一致。唯一需要特别标注的是 PDF 文字描述里写操作 CRC 提到“写数据长度”，但 PDF 时序图和官方例程实际都是固定 5 字节写帧，写 CRC 只算 `cmd+addr+data`。 |
| PDF vs 官方例程 | 官方例程与 PDF 在寄存器地址、SPI 命令、Mode 3、CRC8、ACK、软件复位、FLAG 清除规则上是一致的。官方例程可以作为 PDF 歧义处的落地参考。 |
| PDF vs 主工程 | 主工程的 SPI 协议层和主寄存器表大体正确；偏差集中在 AFE 参数编码、初始化配置完整性、旧 SH367309/I2C 路径残留、参数入口边界保护。 |
| 官方例程 vs 主工程 | 主工程 `sh36735_spi_proto.c` 的读写/复位帧基本等价于官方 `SPIApp.c`，但主工程没有完全照搬官方 `AFE.c` 的寄存器初始化、寄存器巡检和模式封装。 |

后续优先级建议：

1. 以 PDF V0.2C 为寄存器和换算公式的第一真源。
2. PDF 文字与时序图冲突时，以官方 Demo 的实际 SPI 帧为准。
3. `docs/afe/sh36735xx-ai-reference-20260520.md` 作为 AI/工程师快速索引，不作为代码唯一依据。
4. 主工程中暂时只把 `SH36735_reg.h` 当作 SH36735XX 地址真源；`sh3520 driver/sh36735_regs.h` 需要修正后才能继续扩展使用。

## 2. SPI 通信对比

| 项目 | PDF/提取文档 | 官方例程 | 主工程 | 判定 |
| --- | --- | --- | --- | --- |
| SPI 模式 | CPOL=1、CPHA=1，Mode 3，MSB first，SCK 不超过 1 MHz | `Core/Src/spi.c` 配置 `SPI_POLARITY_HIGH`、`SPI_PHASE_2EDGE`、`SPI_FIRSTBIT_MSB` | 软件 SPI 空闲拉高 SCK，下降沿前准备 MOSI，上升后读 MISO；硬件 SPI 文件也按 Mode 3 写，但当前未编译 | 一致 |
| 写命令 | `0x01` | `AFE_WRITE_CMD=0x01` | `SH_SPI_CMD_WRITE_REG` 走 `0x01` | 一致 |
| 读命令 | `0x02` | `AFE_READ_CMD=0x02` | `SH_SPI_CMD_READ_REG` 走 `0x02` | 一致 |
| 软件复位命令 | `0x0B,0xBB,0xCC,CRC,0x00` | `AFE_RESET_CMD=0x0B`，固定 5 字节 | `sh36735_sw_reset()` 固定 5 字节并校验 `0xA5` | 一致 |
| 写帧 | `cmd, reg, data, CRC, dummy`，ACK 为 `0xA5` | `CRC8Calcu(AFETxBuf, 3)`，不含长度 | `SH_WRITE_CRC_INCLUDE_LEN=0`，CRC 算 `cmd,reg,val` | 一致，主工程方向正确 |
| 读帧 | 主机发 `cmd,reg,len,dummy...`，AFE 回 `0xFF,cmd,reg,len,data,CRC` | 校验 `CRC8Calcu(AFERxBuf, RdLen+4)` | CRC 覆盖 `rx0,rx1,rx2,rx3,data` | 一致 |
| 地址范围 | 写 `0x40-0x59`，读 `0x40-0x99` | 例程未集中封装边界 | 主工程限制写 `0x40-0x59`、读 `0x40-0x99` | 主工程更安全 |
| 重试策略 | PDF 只定义通信，不定义软件重试 | 读写默认重试 5 次 | 读写/复位默认重试 5 次 | 一致 |

结论：主工程当前 SPI 协议层可以继续作为 SH36735XX 通信底座使用。后续不要再走 `MTPRead/MTPWrite` 旧接口。

## 3. 寄存器地址表对比

| 地址段 | PDF/提取文档 | 官方例程 `SH36735xx.h` | 主工程 `SH36735_reg.h` | 主工程 `sh36735_regs.h` | 判定 |
| --- | --- | --- | --- | --- | --- |
| `0x40-0x57` 配置/阈值/均衡 | 一致 | 一致 | 一致 | 只列到 `0x50` 附近，部分缺失 | 主工程主表正确 |
| `0x58` | `FLAG1` | `AFE_FLAG1` | `AFE_FLAG1` | 错写为 `SH_REG_BSTATUS1=0x5A` 后续链条偏移 | `sh36735_regs.h` 错 |
| `0x59` | `FLAG2` | `AFE_FLAG2` | `AFE_FLAG2` | 错写为 `SH_REG_BSTATUS2=0x5B` | `sh36735_regs.h` 错 |
| `0x5A` | `FLAG3` | `AFE_FLAG3` | `AFE_FLAG3` | 缺 `FLAG3`，且把 `BSTATUS1` 放到此地址 | `sh36735_regs.h` 错 |
| `0x5B` | `BSTATUS1` | `AFE_BSTATUS1` | `AFE_BSTATUS1` | 把 `BSTATUS2` 放到此地址 | `sh36735_regs.h` 错 |
| `0x5C` | `BSTATUS2` | `AFE_BSTATUS2` | `AFE_BSTATUS2` | 把 `FLAG1` 放到此地址 | `sh36735_regs.h` 错 |
| `0x5D-0x99` 采样/断线 | 一致 | 一致 | 一致 | 基本缺失 | 使用 `SH36735_reg.h` |

结论：`103 + 309/Project/Source/SH36735_reg.h` 与 PDF、官方例程一致。`103 + 309/Project/Source/sh3520 driver/sh36735_regs.h` 当前不能作为状态寄存器地址来源，必须修正或删除重复表。

## 4. 初始化配置对比

| 项目 | PDF 要求/默认语义 | 官方例程 | 主工程现状 | 风险 |
| --- | --- | --- | --- | --- |
| 模式进入 | `SCONF1=0x00/0x55/0xAA/0x33` 分别为 Normal/IDLE/SLEEP/Powerdown | `SH_AFE_WorkModeConfig()` 封装四种模式 | 初始化写 `SCONF1=0x00`，睡眠写 `0xAA` | Normal 进入正确；睡眠缺少返回值和 `BSTATUS2.SLEEP` 确认 |
| `SCONF2` MOS/LTCLR | `LTCLR=1` 后允许写 0 清 FLAG；CHG/DSG MOS 由位控制 | 封装 CHG/DSG/PDSG/PD_CTL | 初始化、清 FLAG 和 MOS 控制都写 `SCONF2` | 多处写寄存器未检查返回，MOS 状态可能和软件镜像分离 |
| `SCONF4.CN` 串数 | 芯片支持 4-20 串，`CN[4:0]` 配置串数 | 从参数取 `CellNum` | 启动路径有限幅到 4-20 | 通信写参数路径仍可绕过限幅，后续 `DataLoad_CellVolt()` 可能越界 |
| `SCONF5` WDT/CADC | PDF 将 WDT、CADC、驱动等控制放在系统配置寄存器 | 官方初始化从参数连续写配置寄存器 | `InitAFE3520_Registers()` 未显式写 `SCONF5`，`SH367309_Enable_AFE_Wdt_Cadc_Drivers()` 为空 | 需要确认当前是否完全依赖复位默认值；函数名与实现不一致 |
| `SCONF6` 保护使能 | TS4/TS3/TS2/TS1、SC、OCD、UV、OV 独立使能 | 由参数写入 | 固定 `0x7F` | TS4 保护未使能；若硬件接了 4 路温度，AFE 硬件保护不覆盖 TS4 |
| OV/UV 阈值和延时 | `0x49/0x4A`、`0x4B/0x4C` 同时包含阈值高位和延时位 | 参数表直接写寄存器 | 主工程先写一次默认值，随后用阈值高位覆盖 `0x49/0x4B`，没有保留 `OVT/UVT` 延时位 | 最终 OVT=140 ms、UVT=490 ms，可能不是预期 |
| 温度阈值 | 温度阈值需按 NTC 分压码写入 | 官方按 NTC 表把温度转寄存器码 | 主工程使用固定电阻值换算后写 OTC/UTC/OTD/UTD | 逻辑可用，但建议改成参数化并统一注释单位 |

结论：主工程不是完整移植官方 `AFE.c`，而是写了一个项目定制的最小初始化。这个方向可以保留，但必须补齐阈值编码、`SCONF5` 意图、TS4 使能策略和写后确认。

## 5. 采样与换算对比

| 项目 | PDF/提取文档 | 官方例程 | 主工程 | 判定 |
| --- | --- | --- | --- | --- |
| 电芯电压 | `Vcell_mV = CELLn * 5 / 32` | 读取 `CELL1H` 起始的 `2*CellNum` 字节 | `U16_SwapEndian(Cell[i]) * 5 >> 5` | 一致 |
| B+/C+ | 先按 `raw*5/32` 得到等效采样，再乘内部比例系数 | 分别读 `VTOPH/VCHGRH` | `((raw*5)>>5)*25` | 公式方向一致，但主工程先截断再乘，低位精度损失 |
| 外部温度 | `TEMPn` 为 ADC 分压码，需要结合 NTC 上拉换算 | 官方按 NTC 表换算 | `1000*TEMP/(32768-TEMP)` 后查 10K 表 | 和 10K 上拉假设一致；建议加 `TEMP>=32768` 防护 |
| CADC 电流 | `CADCD.15` 表示方向，`1` 放电、`0` 充电 | 官方读 `CADCDH/L` | 主工程读 `CADCD` 到 `u16Current`，按 bit15 判断方向 | 方向一致 |
| 电流换算 | `Current = 100 * raw / (29127 * RSENSE)`，工程里用 `g_u32CS_Res_AFE` 表示采样电阻比例 | 官方保留原始码再计算 | 充电 `raw*100*scale/29127`，放电 `raw*scale/29127*100` | 放电路径先除后乘，低电流精度损失 |
| 状态读取 | `FLAG2` 的 VADC/CADC 标志读后自清 | 官方直接读 FLAG1/2 | 主工程读 `0x58-0x5C` 后锁存 `AFE1_LastFlag2ConversionFlags` | 主工程处理更完整 |

结论：主工程采样主链路可用，主要优化点是电流/B+/C+ 计算精度和温度分母边界保护。

## 6. 保护清除与负载释放对比

| 项目 | PDF/提取文档 | 官方例程 | 主工程 | 判定 |
| --- | --- | --- | --- | --- |
| 清 FLAG 前置条件 | 先置 `SCONF2.LTCLR=1` | `SH_AFE_ClearProtectFlag()` 先写 `SCONF2|=0x80` | 新版 `SH_AFE_ClearProtectFlag()` 同样先写 `SCONF2|0x80` | 一致 |
| FLAG1 清除 | 对应 bit 写 0 清除 | 按 mask 写 FLAG1 | 按 mask 写 FLAG1 并读回确认 | 主工程更稳 |
| FLAG2 清除 | `FLAG2[7:2]` 写 0 清，`[1:0]` 为只读自清转换完成标志 | 官方按 mask 写 FLAG2 | 主工程清保护后读回，并保留转换完成锁存 | 主工程更稳 |
| OCD/SC 负载释放 | PDF 支持负载检测/释放相关状态 | 官方有 `SH_AFE_LoadCheckConfig()` | 主工程有 `func_LoadRemove()`，通过 `CRLD_EN=2` 和 `BSTATUS2` 做确认 | 主工程已按项目需求扩展 |

结论：保护清除的主路径是当前工程里做得比较正确的部分。后续要避免旧 `MTPRead/MTPWrite` 版本的 `AFE_IDLE()`、`AFE_Reset()` 被重新启用。

## 7. 当前主工程确认正确的部分

- `sh36735_spi_proto.c` 的写帧、读帧、复位帧、CRC、ACK、重试和边界检查与官方例程一致。
- Keil 工程当前只编译 `sh36735_spi_sw.c`，没有同时编译硬件 SPI 版本，避免了 `sh36735_spi_xfer()` 重定义。
- `SH36735_reg.h` 的寄存器地址与 PDF/官方例程一致。
- `UpdateVoltageFromBqMaximo()` 的 `0x58-0x5C` 状态块读取范围正确，并锁存了 `FLAG2` 自清转换标志。
- 均衡寄存器 `BALANCEH/M/L` 与 PDF 地址顺序一致。
- 新版 `SH_AFE_ClearProtectFlag()` 已按 `LTCLR + 写 0 + 读回确认` 的方向实现。

## 8. 需要修复或确认的差异

### P1：`AFE_PROTECT_param.c` 阈值工具有真实错误

- `sh_encode_ovh_ovt()` 把 `OV[7:0]` 和 `OVT/OV[9:8]` 输出地址写反。
- `sh_decode_occ_occt()` 没有 `return out;`。
- 该文件看起来像调试/参数读取工具，但已经在 Keil 工程中编译，后续如果被调用会产生错误配置或未定义值。

### P1：串数和采样电阻入口需要统一安全函数

- 启动路径会限制 `SeriesNum` 到 4-20。
- 上位机写参数路径直接 `SeriesNum = OtherElement.u16Sys_SeriesNum`，可能绕过限制。
- 多处 `g_u32CS_Res_AFE = num * 1000 / res` 没有 `res=0` 防护。

### P1：旧 SH367309/I2C 路径必须隔离

- `MTPRead/MTPWrite` 仍然存在，语义是旧 TWI/I2C。
- `AFE_Reset()`、`AFE_IDLE()`、`AFE_GetData()`、`AFE_CheckStatus()` 等旧函数仍可能误用旧地址和旧总线。
- 后续开发应把 SH36735XX SPI API 封装成独立 `afe3520_driver` 边界，旧路径改名为 legacy 或用宏禁用。

### P2：修正 `sh36735_regs.h` 的状态寄存器地址

当前该文件的 `FLAG/BSTATUS` 地址与 PDF 相反或偏移。虽然当前未作为主路径使用，但它位于 `sh3520 driver` 目录，后续很容易被误引用。

### P2：明确初始化寄存器完整性

- `SCONF5` 未显式写，`SH367309_Enable_AFE_Wdt_Cadc_Drivers()` 为空，需要确认是否符合硬件目标。
- OV/UV 延时位目前被覆盖成 0，需要确认是否目标就是最快保护。
- TS4 是否接入硬件温度保护，需要在原理图或实测中确认。

### P3：计算精度和边界

- 放电电流应改成 `raw * 100 * scale / 29127` 的统一顺序。
- B+/C+ 可改成 `(raw * 125) >> 5`，避免先截断。
- 温度换算建议防护 `TEMP >= 32768`。

## 9. 后续开发准则

- 新功能只调用 `sh36735_write_reg_u8()`、`sh36735_read_regs()`、`sh36735_sw_reset()`。
- 新增寄存器地址只写在一个真源里，建议保留 `SH36735_reg.h`，删除或修正重复的 `sh36735_regs.h`。
- 所有写配置寄存器的代码都应读回校验；对 `LTCLR`、自清 bit、只读 bit 必须使用 mask 校验。
- 所有来自 EEPROM、上位机、默认参数的串数和采样电阻都必须走统一安全入口。
- 阈值寄存器不要手写裸值；应提供 `OV/UV/OCD/OCC/SC/温度` 编码 helper，并用 PDF 默认值和边界值做最小测试。
- 睡眠、IDLE、Powerdown、MOS 控制都应返回成功/失败，并读 `BSTATUS1/2` 确认状态。
