# SH36735XX AFE 文档摘要与主工程复审

生成日期：2026-05-18

资料来源：

- AFE 文档：`E:/nas sync win/work/SH36735XX CV0.2C.pdf`
- 主工程：`103 + 309`
- 官方例程：`SH3673520+STM32F072CBT6 DemoCode V1.2_20241227`
- 当前分支：`codex/afe-spi-refactor-debug`

## 1. 结论

按 AFE 文档和官方例程复核后，当前主工程的 SH3673520 SPI 底层通信方向是对的：软件 SPI 为 Mode 3，SCK 空闲高电平，按 MSB first 发送，读/写/软件复位帧格式、ACK、回显和 CRC8 校验已经与官方例程一致。上一轮 ST-Link/OpenOCD 板上验证也能读回 AFE 配置和电芯数据。

当前更大的风险不在“SPI 时序一定错”，而在 AFE 寄存器配置闭环和主工程架构边界：

- AFE 寄存器镜像结构里 `BALANCEM/BALANCEL` 顺序与 PDF/官方例程不一致。
- 串数来源不统一，AFE `SCONF4` 使用编译期 `SNum=19`，业务层 `SeriesNum` 又在 EEPROM 初始化后覆盖。
- 初始化只写部分寄存器，且只读回 `SCONF4`，没有像官方例程一样对关键 RAM 配置形成完整校验闭环。
- 读写 API 还缺少寄存器地址范围和读取长度保护，当前调用没触发，但接口本身不够稳。
- 周期性读取 `FLAG2` 会自动清除 `VADC_FLG/CADC_FLG`，如果后续使用 ALARM/转换完成标志，要明确这个副作用。

## 2. AFE 文档重点

### 2.1 SPI 基本要求

文档第 25~27 页给出的通信约束：

- SH36735XX 只作为 SPI 从机，MCU 是主机。
- 工作频率最大 1 MHz。
- 三线同步全双工，信号为 `SDI`、`SDO`、`SCK`，另有低有效 `CS`。
- `CS` 为高时 AFE 忽略 `SCK`，`SDO` 高阻；`CS` 拉低后 SPI 模块开启。
- `CPOL=1`、`CPHA=1` 固定，即 SPI Mode 3。
- `CS` 在一个字节传输期间必须保持低电平；一帧通信结束后拉高。
- `CS` 拉高超过 `tSPIDIS` 后 SPI 模块关闭；`CS` 拉低但长时间没有 SCK 下降沿，超过 `tSPIRST` 后 SPI 模块复位。

第 51 页时序参数对固件最关键的是：

| 项目 | 要求 |
| --- | --- |
| SCK 频率 | 不超过 1 MHz |
| SCK 低电平时间 `TCL` | 最小 500 ns |
| SCK 高电平时间 `TCH` | 最小 500 ns |
| `CS` 建立/保持 | 最小 100 ns |
| `tSPIDIS` | 典型 30 ns，最大 50 ns |
| `tSPIRST` | 典型 1 s |

主工程软件 SPI 每 bit 拉低约 1 us、拉高约 1 us，等效约 500 kHz，满足文档时序余量。

### 2.2 SPI 帧格式

写 RAM 寄存器，地址范围 `0x40..0x59`，固定写 1 字节：

```text
MOSI: 0x01, RegAddr, WriteData, CRC8, 0x00
MISO: 0xFF, 0x01, RegAddr, WriteData, 0xA5 或 0xFF
```

读 RAM 寄存器，地址范围 `0x40..0x99`，长度不包含最后的 CRC 字节：

```text
MOSI: 0x02, RegAddr, Len, 0x00, Len 个 0x00, 0x00
MISO: 0xFF, 0x02, RegAddr, Len, Len 个 Data, CRC8
```

软件复位：

```text
MOSI: 0x0B, 0xBB, 0xCC, CRC8, 0x00
MISO: 0xFF, 0x0B, 0xBB, 0xCC, 0xA5 或 0xFF
```

CRC8 多项式为 `0x07`，初始值 `0x00`。文档文字里写操作 CRC 描述提到了“写数据长度”，但写时序图和官方例程都没有长度字节，官方例程按 3 字节 `0x01, RegAddr, Data` 计算 CRC。主工程当前 `SH_WRITE_CRC_INCLUDE_LEN=0` 与官方例程一致，应以官方例程和板上实测为准。

### 2.3 关键寄存器

寄存器列表从第 28 页开始，主工程直接相关的 RAM 寄存器如下：

| 地址 | 名称 | 作用 |
| --- | --- | --- |
| `0x40` | `SCONF1` | 工作模式：Normal、IDLE、SLEEP、Powerdown |
| `0x41` | `SCONF2` | MOS 控制、Charge Pump、Powerdown、`LTCLR` |
| `0x42` | `SCONF3` | 充电器/负载唤醒、负载检测、断线检测 |
| `0x43` | `SCONF4` | 串数 `CN[4:0]`、预放电检测时间 |
| `0x44` | `SCONF5` | `MOS_EN`、`OCC_EN`、`CADC_EN`、`WDT_EN/WDT` |
| `0x45` | `SCONF6` | `TS4..TS1`、`SC/OCD/UV/OV` 保护使能 |
| `0x46` | `SCONF7` | 负载检测上拉、IDLE 下 CADC 周期、负载检测压差 |
| `0x47..0x54` | 阈值区 | 断线、ALARM、OV/UV/OCD/SC/OCC/温度阈值 |
| `0x55..0x57` | `BALANCEH/M/L` | 20 串均衡控制位 |
| `0x58..0x5A` | `FLAG1/2/3` | 保护、唤醒、WDT、VADC/CADC 标志 |
| `0x5B..0x5C` | `BSTATUS1/2` | MOS、充放电、负载、均衡、模式状态 |
| `0x5D..0x99` | ADC/状态数据 | 温度、电流、电芯、电流累计、总压、C+、断线 |

需要特别注意：

- `SCONF2.LTCLR=1` 后才允许写 0 清除 `FLAG1/FLAG2` 的保护标志，清除后 AFE 会自动清 `LTCLR`。
- `SCONF4.CN` 对 SH3673520，`0x13` 表示 19 串，`0x14` 表示 20 串，其他无效值按 20 串处理。
- `SCONF5` 默认值为 `0x38`，默认 `MOS_EN=1`、`OCC_EN=1`、`CADC_EN=1`、`WDT_EN=0`。
- `SCONF6` 默认值为 `0xFF`，主工程写 `0x7F` 会关闭 `TS4_EN`，其他温度/电流/电压保护仍开启。
- `BALANCEH/M/L` 地址顺序固定是 `0x55/0x56/0x57`，对应高、中、低 3 字节。
- `FLAG2` 中 `VADC_FLG`、`CADC_FLG` 在读取 `FLAG2` 后会自动清零。

## 3. 官方例程对照

官方 SPI 初始化位于 `Core/Src/spi.c`：

- `SPI_MODE_MASTER`
- `SPI_DIRECTION_2LINES`
- `SPI_DATASIZE_8BIT`
- `SPI_POLARITY_HIGH`
- `SPI_PHASE_2EDGE`
- `SPI_NSS_SOFT`
- `SPI_FIRSTBIT_MSB`
- 硬件 CRC 关闭

官方 SPI 协议位于 `BMS_Drivers/Src/SPIApp.c`：

- `SH_AFE_SPI_Write()` 写 5 字节，校验 `0xFF/cmd/reg/data/0xA5`。
- `SH_AFE_SPI_Read()` 读 `Len+5` 字节，校验 `0xFF/cmd/reg/len/CRC8`。
- `SH_AFE_SPI_SoftReset()` 发送 5 字节，最后一个 `0x00` 用于读 ACK。
- 对外 `SH_AFE_Write/Read/SoftReset` 都重试 5 次，失败延时 1 ms。

官方 AFE 初始化位于 `BMS_Drivers/Src/AFE.c`：

- `SH_AFE_RegisterInit()` 从参数区连续写 `0x40..0x54`，共 21 个 RAM 配置寄存器。
- `SH_AFE_RegisterCheck()` 周期读回 `SCONF2` 开始的配置区，比较 RAM 配置是否被异常改写。
- 比较时跳过 `SCONF2.LTCLR` 和 `SCONF3.OWD_TRG` 这类会被 AFE 自动改变的位。

官方结构体 `Common.h` 中均衡字段顺序是 `AFEBALANCEH, AFEBALANCEM, AFEBALANCEL`，与 PDF 寄存器地址一致。

## 4. 主工程当前实现

### 4.1 实际编译路径

Keil 工程中 `sh36735_spi_hw.c` 存在但 `IncludeInBuild=0`，当前实际编译的是 `sh36735_spi_sw.c`。也就是说现在板上运行的是软件 SPI，不是 STM32 硬件 SPI。

启动路径：

1. `InitDevice()` 初始化 IO、EEPROM、CAN、串口、MOS/SOC 等。
2. `bsp_InitSPIBus()` 配置 SPI GPIO。
3. `sh36735_spi_sw_init()` 拉高 `CS/SCK/MOSI`。
4. `InitAFE3520_Registers(0, 0)` 写 AFE 寄存器。
5. `InitVar()` 后续从 EEPROM 恢复 `SeriesNum` 等业务参数。

采样路径：

1. `App_AFEGet()` 每 200 ms 分槽调用 `UpdateVoltageFromBqMaximo()`。
2. `UpdateVoltageFromBqMaximo()` 分块读 `0x40..0x46`、`0x47..0x57`、`0x58..0x5C`、`0x5D..0x96`。
3. SPI 读失败时返回错误 bitmask，置 `ERROR_SPI`，业务层停止使用半更新数据。
4. 读成功后转换电芯电压、温度、CADC 原始电流、BAT+/C+ 电压。

## 5. 复审发现

### P1：寄存器镜像里均衡中/低字节顺序错

主工程 `I2C_AFE1.h` 的 `AFEDATA` 字段顺序为：

```c
uint8_t BALANCEH;
uint8_t BALANCEL;
uint8_t BALANCEM;
```

但 PDF 和官方例程均为 `BALANCEH, BALANCEM, BALANCEL`，地址分别是 `0x55, 0x56, 0x57`。主工程 `UpdateVoltageFromBqMaximo()` 又会从 `0x47..0x57` 连续读入该结构体，所以 `Registers_AFE1.BALANCEM` 和 `Registers_AFE1.BALANCEL` 的镜像值会互换。

当前均衡写入函数直接按宏 `AFE_BALANCEH/M/L` 写寄存器，因此写均衡不一定受这个结构体顺序影响；但任何读回诊断、上报、调试或未来校验都会被这个错位误导。建议直接修正结构体顺序，并重新编译确认没有依赖旧错位的代码。

### P1：串数来源不统一

当前 `SNum` 在 `DataDeal.h` 固定为 19，`InitAFE3520_Registers()` 用 `SNum` 写 `SCONF4`，采样转换也只循环 `SNum` 个电芯。另一方面，`SeriesNum` 上电初值为 16，在 `InitVar()` 中由 EEPROM 参数 `OtherElement.u16Sys_SeriesNum` 覆盖，均衡和部分业务逻辑使用 `SeriesNum`。

风险是：

- 如果 EEPROM 参数不是 19，AFE 串数、采样转换、均衡、保护阈值和上报串数可能不一致。
- 如果产品真是 20 串，当前采样转换只更新 19 个 cell。
- 如果产品是 16 串，但 AFE 配成 19 串，未使用 VC 端口的硬件处理、断线/保护阈值都会受影响。

建议在 EEPROM 参数恢复后再统一初始化 AFE，或者把 `SeriesNum` 的可信来源前移，并让 `SCONF4`、采样循环、均衡循环和上报都用同一个经过范围校验的串数。

### P1：AFE 初始化未形成官方式配置闭环

主工程 `InitAFE3520_Registers()` 当前写了 `SCONF1/2/3/4/6`、OV/UV、OCD2、OCC、温度阈值，最后只读回 `SCONF4`。官方例程连续写 `0x40..0x54` 并周期读回配置区。

具体风险：

- `SCONF5` 未显式写入，依赖 AFE 默认值 `0x38`。如果发生软件复位、异常掉电、后续配置改动，代码没有统一的期望配置表。
- `OCD1V/OCD1T`、`SCV/SCT`、`OWV/ALARM`、`SCONF7` 等关键寄存器没有在当前初始化中完整覆盖。
- `SCONF6` 写入值变量名为 `mos_en`，实际写的是保护使能寄存器，`0x7F` 会关闭 `TS4_EN`，这可能是有意少用 4 路温度，也可能是误配置。
- 只读回 `SCONF4` 无法发现其他 RAM 寄存器写失败或运行中被异常改写。

建议建立 `sh36735xx_config[]` 配置表，统一写 `0x40..0x54`，并按官方方式读回校验；对 `LTCLR/OWD_TRG` 这类自清或触发位做 mask 比较。

### P1：SPI 读写 API 缺少地址和长度边界

`sh36735_read_regs(reg, buf, n)` 目前只检查 `buf != NULL` 和 `n != 0`，没有限制：

- 读起始地址必须在 `0x40..0x99`。
- `reg + n - 1` 不能超过 `0x99`。
- 写地址必须在 `0x40..0x59`。
- 最大读长度应按主工程实际需求限制，避免 255 字节栈数组成为隐藏风险。

当前调用最大读到 `0x5D..0x96`，没有越界；但这个接口后续被误用时，会把协议错误变成 SPI 失败或栈压力问题。建议增加 `SH_REG_READ_MIN/MAX`、`SH_REG_WRITE_MIN/MAX`、`SH_SPI_READ_MAX_LEN`。

### P2：周期读 `FLAG2` 会清除转换完成标志

PDF 明确说明读取 `FLAG2` 会自动清除 `VADC_FLG` 和 `CADC_FLG`。主工程每次采样都会读 `0x58..0x5C`，因此这两个转换完成标志会被周期性清掉。

如果系统只是轮询 ADC 数据，这没有明显问题；如果后续要用 ALARM 或 `VADC_FLG/CADC_FLG` 判断新数据到达，需要把“谁负责读 FLAG2、谁消费这些标志”设计清楚。

### P2：均衡 3 字节写入失败可能留下半更新状态

`CB_AfeWriteBalanceMaskU24()` 依次写 `BALANCEH/M/L`，任一失败会重试整组三字节，并且只有全部成功才更新软件状态。这比原来可靠，但仍存在一种失效模式：如果某次 H/M 已成功、L 失败，且最终重试仍失败，AFE 内部可能已经保留了部分新均衡位。

建议失败后尝试写全 0 关闭均衡，或增加均衡寄存器读回校验；在 SPI 错误持续期间，业务层应按“硬件均衡状态不可信”处理。

### P2：保护清标志函数缺少完整返回检查

`SH_AFE_ClearProtectFlag()` 会先写 `SCONF2.LTCLR=1`，再写 `FLAG1/FLAG2` 清标志。当前第一步写 `SCONF2` 的返回值被忽略，且本地 `Registers_AFE1.sonf2` 镜像没有同步 AFE 自动清 `LTCLR` 的行为。

建议：

- `SCONF2` 写失败时直接返回失败。
- 清标志后读回 `FLAG1/FLAG2` 和 `SCONF2`。
- 本地镜像只作为缓存，不作为清标志成功的证据。

### P2：旧 SH367309 命名和空函数会误导维护

主工程大量新 SH3673520 SPI 数据仍落在 `SH367309_Read_AFE1`、`I2C_AFE1.c`、`SH367309_Func.c` 这些旧命名里。`SH367309_Enable_AFE_Wdt_Cadc_Drivers()` 注释写着启用 MOS/WDT/CADC，但函数体为空。

这不会直接导致当前 SPI 读写失败，但会让后续维护人员误判“已经启用 WDT/CADC/MOS 驱动”。建议建立 `sh36735xx_afe.c/h` 适配层，把旧 SH367309 兼容结构和新 AFE 驱动边界拆开。

### P2：禁用测试块里仍有赋值判断错误

`DataDeal.c` 的 `#if 0` 测试代码中存在：

```c
if (Registers_AFE1.sonf3.bits.CRLD_EN = 2)
if (Registers_AFE1.sonf3.bits.CRLD_EN = 0)
```

该代码当前不参与编译，不影响当前固件；但它属于危险测试代码，后续如果打开会直接改变 `CRLD_EN`。建议删除这段禁用测试代码，或修成显式比较。

## 6. 当前 SPI 通信判断

按 PDF 和官方例程复核，主工程当前 SPI 通信没有发现以下问题：

- Mode 错误：当前软件 SPI 为 CPOL=1、CPHA=1。
- CRC 多项式错误：当前为 `0x07`、初值 `0x00`。
- 写 ACK 读错位置：当前已在第 5 字节读 ACK。
- 读帧缺少 CRC 校验：当前已校验 `0xFF/cmd/reg/len/data/crc`。
- 软件复位少 dummy：当前已发最后一个 `0x00` 读取 ACK。
- 硬件 SPI 和软件 SPI 同时编译：Keil 工程中硬件 SPI 文件被排除，软件 SPI 文件被包含。

所以如果板上仍表现为 AFE SPI 不稳定，应优先排查硬件层和系统层：

- `CS/SCK/SDI/SDO` 是否接线和命名一致。
- `SDO` 上拉/输入模式、电平幅值是否符合 VCC_MCU。
- `CS` 是否被其他旧代码或调试代码抢占。
- SPI 线是否有串阻、上拉、地弹、电池高压干扰导致边沿异常。
- `SCONF4` 串数和实际硬件短接/未用 VC 端口处理是否一致。

## 7. 建议整改顺序

1. 先修 `AFEDATA` 中 `BALANCEM/BALANCEL` 顺序，并增加 SPI 寄存器地址/长度边界检查。
2. 把 AFE 初始化改成配置表，覆盖 `0x40..0x54`，并增加读回校验。
3. 统一串数来源，让 `SCONF4`、采样、均衡、保护和上报都使用同一个经过校验的值。
4. 增加 SPI 错误计数、最后失败命令/地址/长度记录，方便 ST-Link 或串口定位间歇性问题。
5. 删除已不用的 AFE/I2C/MTP 测试代码，保留兼容代码时加清晰边界注释。
6. 做台架验证：冷启动、软复位、19/20 串配置、读写 CRC 错误、保护清标志、均衡打开/关闭、短路恢复、长时间 24h 采样。

## 8. 验证状态

上一轮已完成：

- Keil 命令行构建：0 errors，仍有 50 个 warnings。
- ST-Link/OpenOCD 下载和短时间运行验证。
- 读回 `SCONF4=0x13`，与当前 `SNum=19` 一致。
- 可通过 SPI 读回 AFE 状态和电芯数据。

本轮新增的是基于 PDF 和官方例程的文档复审，没有重新改写代码，也没有推送远端。
