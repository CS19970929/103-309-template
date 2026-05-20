# 当前项目 AFE 代码梳理与问题清单

审阅范围：

- 主工程：`103 + 309/Project/Source`
- 官方参考：`SH3673520+STM32F072CBT6 DemoCode V1.2_20241227`
- 数据手册：`SH36735XX CV0.2C.pdf`

本轮只生成文档和审计结论，没有修改功能代码。

## 1. 当前 AFE 代码拓扑

### 1.1 SPI 底层

| 文件 | 作用 | 当前状态 |
| --- | --- | --- |
| `sh3520 driver/sh36735_spi_proto.c` | SH36735XX SPI 帧、CRC8、ACK、读回显、重试、地址边界 | 当前主通信路径，符合官方 Demo 的帧格式 |
| `sh3520 driver/sh36735_spi_sw.c` | 软件 SPI，Mode 3 时序，MSB first | Keil 工程启用 |
| `sh3520 driver/sh36735_spi_hw.c` | 硬件 SPI1 实现 | Keil 工程里 `IncludeInBuild=0`，当前不编译 |
| `bsp_spi_bus.c` | 初始化 SCK/MOSI/MISO GPIO | `main.c` 的 `InitDevice()` 中调用 |
| `conf/conf.c` | 初始化 CS GPIO | `InitIO()` 已初始化 `PIN_CS_SPI` |

主工程在 `main.c` 中调用：

- `bsp_InitSPIBus()`
- `sh36735_spi_sw_init()`
- `InitAFE3520_Registers(0, 0)`

### 1.2 AFE 初始化

入口：`main.c::InitAFE3520_Registers()`

当前写入：

- `SCONF1=0x00` 进入 Normal。
- `SCONF2`：`LTCLR=1`，`PD_EN=0`，`CHGMOS/DSGMOS` 由参数控制，`PUMP_EN=0`。
- `SCONF4.CN=SeriesNum`。
- `SCONF3.CRLD_EN=0`。
- `SCONF6=0x7F`，使能 TS3/TS2/TS1、SC、OCD、UV、OV，未使能 TS4。
- OV/UV/OCD2/OCC 和温度阈值。
- 初始化完成后调用 `SH367309_DriverMos_Ctrl(GPIO_CHG, 1)` 和 `SH367309_DriverMos_Ctrl(GPIO_DSG, 1)`。

### 1.3 周期采样

入口：`DataDeal.c::App_AFEGet()`

执行周期：

- 依赖 `b1Sys200msFlag3`，并避开串口发送窗口和 EEPROM 写窗口。
- 调 `UpdateVoltageFromBqMaximo()` 读取 AFE。
- 失败后进入 `AFE_UpdateFailDeal()`，连续失败会清陈旧数据，60 s 级别失败会进入深度休眠。
- 成功后更新电芯、电流、温度、均值、最大/最小值，并执行 AFE 保护清除、负载释放检测和 MOS 控制。

`I2C_AFE1.c::UpdateVoltageFromBqMaximo()` 当前读取：

- `0x40-0x46` 到配置结构。
- `0x47-0x57` 到阈值和均衡结构。
- `0x58-0x5C` 到 FLAG/BSTATUS；读取 `FLAG2` 后锁存 `AFE1_LastFlag2ConversionFlags`。
- `0x5D-0x96` 到温度、电芯、CADC、B+、C+。

### 1.4 均衡

入口：`Cell_balance.c::App_CellBalance()`

- 按 1 s 周期执行。
- 只在无保护、AFE/SPI/CBC 错误均未置位、充放电电流小、最低电芯电压和压差满足条件时开启。
- 写 `BALANCEH/BALANCEM/BALANCEL`，顺序和 PDF 一致。
- 3 次刷新一次已经开启的均衡，覆盖 AFE 30.38 s 自动关闭的问题。

### 1.5 保护清除和负载释放

入口：

- `SH367309_Func.c::SH_AFE_ClearProtectFlag()`
- `DataDeal.c::func_LoadRemove()`

当前逻辑：

- 清保护前先写 `SCONF2.LTCLR=1`。
- FLAG1/FLAG2 对应保护 bit 写 0。
- 清 FLAG2 后读回并保留 `VADC/CADC` 自清标志。
- OCD/SC 类保护进入负载释放状态机，使用 `SCONF3.CRLD_EN=2` 检测负载未连接。

## 2. 已确认做得正确的部分

- SPI 写帧、读帧、软件复位帧与官方 Demo 一致。
- SPI 读写有地址范围限制：写 `0x40-0x59`，读 `0x40-0x99`。
- 读操作 CRC 覆盖 `0xFF + CMD + addr + len + data`，符合 PDF。
- 软件 SPI 时钟空闲高，下降沿前准备 MOSI，上升后读 MISO，匹配 Mode 3。
- `FLAG2.VADC_FLG/CADC_FLG` 读后自清的问题已经在采样函数里锁存。
- 均衡寄存器的 H/M/L 顺序已经与 PDF 对齐。
- Keil 工程只编译软件 SPI，硬件 SPI 当前排除，避免两个 `sh36735_spi_xfer()` 重定义。

## 3. Bug 和风险

### P1：`AFE_PROTECT_param.c` 存在实质编码/返回值错误

位置：

- `103 + 309/Project/Source/AFE_PROTECT_param.c:133`
- `103 + 309/Project/Source/AFE_PROTECT_param.c:223`

问题：

- `sh_encode_ovh_ovt()` 把 `OV[7:0]` 写到输出的 `reg49`，再把 `OVT[2:0]` 和 `OV[9:8]` 写到输出的 `reg4A`。PDF 定义相反：`0x49` 是 `OVT[2:0] + OV[9:8]`，`0x4A` 是 `OV[7:0]`。如果后续用它生成 OV 配置，会写错过充阈值和延时。
- `sh_decode_occ_occt()` 返回类型是 `sh_occ_occt_t`，但函数体没有 `return out;`。当前 `test_read_afe_param()` 调用它后 `afe_protect.occ` 是未定义值，编译器也应给出警告。

建议：

- 修正 `sh_encode_ovh_ovt()` 输出字节命名和赋值。
- 补齐 `return out;`。
- 同时把 UV/OV 相关注释从旧的 `reg49/reg4A` 文案改成真实地址，避免二次误用。

### P1：串数可被通信路径绕过 4-20 限制

位置：

- `103 + 309/Project/Source/Sci_Upper.c:2668`
- `103 + 309/Project/Source/Sci_Upper.c:2939`
- `103 + 309/Project/Source/DataDeal.c:104`
- `103 + 309/Project/Source/I2C_AFE1.c:940`

问题：

- `main.c` 启动时用 `AFE3520_NormalizeSeriesNum()` 限制串数，但 `Sci_Upper.c` 直接 `SeriesNum = OtherElement.u16Sys_SeriesNum`。
- `UpdateVoltageFromBqMaximo()` 读 AFE 数据时把 cell count 限制到 20，但 `DataLoad_CellVolt()` 按 `SeriesNum` 遍历 `SH367309_Read_AFE1.u16VCell[i]`。如果上位机写入大于 20 的串数，存在越界读取风险。

建议：

- 抽出公共函数，例如 `AFE3520_SetSeriesNumSafe()`，所有写入串数的路径都用它。
- 对 `OtherElement.u16Sys_SeriesNum` 做 4-20 范围约束，必要时回写 EEPROM 默认值。
- `DataLoad_CellVolt()` 自身也要二次限幅，避免未来路径绕过。

### P1：采样电阻参数存在除零风险

位置：

- `103 + 309/Project/Source/main.c:232`
- `103 + 309/Project/Source/Sci_Upper.c:2671`
- `103 + 309/Project/Source/Sci_Upper.c:2940`
- `103 + 309/Project/Source/SH367309_DataDeal.c:55`
- `103 + 309/Project/Source/SH367309_DataDeal.c:130`

问题：

- 多处直接计算 `g_u32CS_Res_AFE = OtherElement.u16Sys_CS_Res_Num * 1000 / OtherElement.u16Sys_CS_Res`。
- 如果 EEPROM 或上位机参数把 `u16Sys_CS_Res` 写成 0，会除零。

建议：

- 增加 `AFE3520_UpdateSenseResScaleSafe()`，统一检查分母、范围和默认值。
- 参数非法时置错误并回退默认采样电阻，不继续写 AFE 过流阈值。

### P1：旧 SH367309/I2C 路径仍在工程中，误启用会访问错误总线

位置：

- `103 + 309/Project/Source/I2C_AFE1.c:721`
- `103 + 309/Project/Source/I2C_AFE1.c:788`
- `103 + 309/Project/Source/SH367309_Func.c:46`
- `103 + 309/Project/Source/SH367309_DataDeal.c:291`

问题：

- 文件名和函数名仍大量沿用 `I2C_AFE1`、`MTPRead`、`MTPWrite`、`SH367309`。
- `MTPRead/MTPWrite` 走旧 TWI/I2C 风格，`AFE_ID=0x34`，不适用于 SH36735XX SPI。
- 当前主采样路径已经改到 `sh36735_read_regs()`，但 `InitAFE1()`、`AFE_Reset()`、`SH367309_UpdataAfeConfig()`、`AFE_IDLE()` 等旧函数还存在。如果后续被重新启用，会和当前 SPI 架构冲突。

建议：

- 把旧 I2C 路径明确标记为 legacy，或用编译宏默认禁用。
- SH36735XX 的复位统一改用 `sh36735_sw_reset()`。
- 新增 `afe3520_driver.c/h`，把初始化、模式切换、保护清除、采样读取集中到一个 SPI 驱动边界内。

### P2：`sh36735_regs.h` 的 FLAG/BSTATUS 地址与 PDF 不一致

位置：`103 + 309/Project/Source/sh3520 driver/sh36735_regs.h:29`

问题：

- 文件中定义 `SH_REG_BSTATUS1=0x5A`、`SH_REG_BSTATUS2=0x5B`、`SH_REG_FLAG1=0x5C`、`SH_REG_FLAG2=0x5D`。
- PDF 定义是 `FLAG1=0x58`、`FLAG2=0x59`、`FLAG3=0x5A`、`BSTATUS1=0x5B`、`BSTATUS2=0x5C`。
- 当前这些宏未被工程引用，所以不是运行时问题，但后续使用会直接读错寄存器。

建议：

- 删除该文件里错误的状态/标志宏，或全部改为与 `SH36735_reg.h` 一致。
- 只保留一个寄存器地址来源，避免双表漂移。

### P2：初始化阈值写法容易误解，保护延时可能不是预期值

位置：`103 + 309/Project/Source/main.c:149`

问题：

- `InitAFE3520_Registers()` 先写 `AFE_OVT_OVH=0x03`、`AFE_OVL=0x50`，随后又用 `ov_thd` 覆盖 `AFE_OVT_OVH/AFE_OVL`。
- 后一次写 `AFE_OVT_OVH` 只写入 `OV[9:8]`，没有写 `OVT[2:0]`，因此 OVT 延时为 `000=140 ms`。
- UVT 同样写成 `000=490 ms`。

建议：

- 如果目标就是快速保护，增加注释说明。
- 如果目标是 PDF 默认 0.98 s，应把 `OVT/UVT` bits 一起编码到 `0x49/0x4B`。
- 建议用专门编码函数生成 `OVT_OVH/OVL` 和 `UVT_UVH/UVL`，避免手写位操作。

### P2：`SCONF6=0x7F` 未使能 TS4 温度保护

位置：`103 + 309/Project/Source/main.c:7`

问题：

- `0x7F` 使能 TS3/TS2/TS1、SC、OCD、UV、OV，但 `TS4_EN=0`。
- 当前采样读取了 TEMP4，也有温度表转换。若硬件实际接了 4 路温度，TS4 只采样不参与 AFE 硬件温度保护。

建议：

- 按硬件确认 TS4 是否接入。
- 若需要 4 路温度保护，改为 `0xFF` 或显式写出位宏组合。

### P2：若干 SPI 写没有返回值处理

位置：

- `103 + 309/Project/Source/SH367309_Func.c:323`
- `103 + 309/Project/Source/SH367309_Func.c:72`

问题：

- `SH367309_DriverMos_Ctrl()` 写 `SCONF2` 后不检查返回值，也不读回确认 MOS 控制位。
- `AFE_Sleep()` 写 `SCONF1=0xAA` 不检查是否成功，也没有确认 `BSTATUS2.SLEEP`。

建议：

- 把 MOS 控制和模式切换统一改成 `bool` 返回，并在失败时置 `ERROR_SPI`。
- SLEEP 前后读取 `BSTATUS2`，必要时重试或禁止继续进入 MCU 低功耗。

### P3：电流和总压计算需要统一精度

位置：

- `103 + 309/Project/Source/DataDeal.c:356`
- `103 + 309/Project/Source/I2C_AFE1.c:971`

问题：

- 放电电流公式当前是 `raw * scale / 29127 * 100`，先除后乘会损失低电流精度；充电路径是 `raw * 100 * scale / 29127`。
- B+/C+ 当前先做 `raw * 5 >> 5` 再乘 25，低位也会先被截断。

建议：

- 电流统一成有符号 raw helper，使用 `raw * 100 * scale / 29127`。
- B+/C+ 改成 `(raw * 125) >> 5` 或同等宽度的 32-bit 一次性计算。

## 4. 建议整改顺序

1. 修 `AFE_PROTECT_param.c` 的 `return` 和 OV 编码错误。
2. 统一串数和采样电阻参数的安全入口，堵住越界和除零。
3. 清理 `sh36735_regs.h` 错误地址，确立 `SH36735_reg.h` 为唯一真源。
4. 把 `InitAFE3520_Registers()` 的阈值/延时编码函数化。
5. 把旧 `MTPRead/MTPWrite/SH367309` 路径隔离为 legacy 或迁移为 SPI 驱动。
6. 为 MOS 控制、SLEEP、IDLE、Powerdown 增加返回值检查和状态读回。

## 5. 后续验证清单

- Keil 编译零 error、零新增 warning，重点看 `AFE_PROTECT_param.c`。
- 逻辑分析仪验证 SPI Mode 3、CS 拉低整帧、SCK 小于 1 MHz。
- 连续读 `0x58-0x5C`，确认 FLAG1/2/BSTATUS1/2 地址和结构体字段一致。
- 读取 `FLAG2` 后确认 `VADC/CADC` 自清标志被软件锁存。
- 写 `SCONF4.CN` 后读回，确认 4-20 串都按 PDF 映射。
- 写 `BALANCEH/M/L` 后读回，确认 Cell1 对应 `BALANCEL.bit0`，Cell20 对应 `BALANCEH.bit3`。
- 触发 OV/UV/OCD/OCC/SC/温度保护，确认 `LTCLR + 写 0` 能清除对应 FLAG，并且 MOS 状态符合预期。
- 上位机写非法串数和非法采样电阻，确认不会越界或除零。
