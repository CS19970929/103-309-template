# AFE 五项风险说明与本轮修复

生成日期：2026-05-18

本文解释前面复审里提到的五个风险到底是什么、影响在哪里、这轮如何处理、还剩什么需要实物验证。

## 1. `BALANCEM/BALANCEL` 镜像顺序错

### 问题是什么

SH36735XX 文档和官方例程的均衡寄存器顺序是：

```text
0x55 BALANCEH
0x56 BALANCEM
0x57 BALANCEL
```

主工程 `AFEDATA` 结构体原来写成：

```c
uint8_t BALANCEH;
uint8_t BALANCEL;
uint8_t BALANCEM;
```

`UpdateVoltageFromBqMaximo()` 会从 `0x47..0x57` 连续读 AFE 寄存器到 `Registers_AFE1`。结构体顺序错时，`0x56` 会落到 `BALANCEL`，`0x57` 会落到 `BALANCEM`。

### 影响

均衡写入函数直接写 `AFE_BALANCEH/M/L` 地址，所以写均衡本身不一定错；但读回镜像、调试观察、以后做读回校验时会把中低字节看反。

### 本轮修复

已把 `I2C_AFE1.h` 中 `AFEDATA` 顺序改成：

```c
uint8_t BALANCEH;
uint8_t BALANCEM;
uint8_t BALANCEL;
```

## 2. `SNum/SeriesNum` 串数来源不统一

### 问题是什么

主工程有多个串数来源：

- `SNum`：编译期宏，当前为 19。
- `SeriesNum`：运行期全局变量，上电默认 16。
- `OtherElement.u16Sys_SeriesNum`：EEPROM 参数。
- `SCONF4.CN`：AFE 内部串数配置。

原启动顺序里，AFE 初始化发生在 `InitVar()` 恢复 EEPROM 参数之前，所以 AFE `SCONF4` 固定使用 `SNum`，业务层后面又用 EEPROM 覆盖 `SeriesNum`。

### 影响

如果 EEPROM 串数不是 19，就会出现 AFE 配置、采样循环、均衡循环、保护判断、上报串数不一致。20 串时还会因为采样转换只循环 `SNum` 而漏掉第 20 串。

### 本轮修复

- AFE 初始化从 `InitDevice()` 移到 `main()` 中 `InitVar()` 之后。
- 新增 `AFE3520_NormalizeSeriesNum()`，只接受 4..20 串，非法值回退到 `SNum`。
- 新增 `AFE3520_SyncSeriesNum()`，同步 `SeriesNum` 和 `OtherElement.u16Sys_SeriesNum`。
- `SCONF4` 改为使用规范化后的 `SeriesNum`。
- `UpdateVoltageFromBqMaximo()` 的电芯转换循环从 `SNum` 改成 `SeriesNum`，并清零未使用 cell 缓存。

## 3. 初始化只读回 `SCONF4`

### 问题是什么

原 `InitAFE3520_Registers()` 写了多个寄存器，但最后只读回 `SCONF4`。如果 `SCONF2/SCONF3/SCONF6/OV/UV/OCD/OCC/温度阈值` 写失败，初始化函数不一定能发现。

### 影响

AFE 看起来完成初始化，但实际可能只有串数正确，其他保护阈值或使能位没写进去。官方例程的做法是写配置区并做寄存器检查，主工程原来闭环不够。

### 本轮修复

`AFE3520_WriteRegChecked()` 现在每写一个寄存器后立即读回同地址，并比较写入值。这样初始化阶段不再只依赖 `SCONF4`，而是对每个写入寄存器形成写后读回校验。

## 4. SPI API 缺少地址/长度边界

### 问题是什么

SH36735XX 文档规定：

- 可写 RAM 地址：`0x40..0x59`
- 可读 RAM 地址：`0x40..0x99`

原 `sh36735_write_reg_u8()` 不检查写地址，`sh36735_read_regs()` 只检查 `buf` 和 `n != 0`，没有检查 `reg + n - 1` 是否越过 `0x99`。

### 影响

正常调用目前没有越界，但后续误传地址或长度时，底层会照样发 SPI 帧，错误会表现成通信失败、栈压力或难定位的协议异常。

### 本轮修复

`sh36735_spi_proto.c` 已新增：

- 写地址范围检查：`0x40..0x59`
- 读地址范围检查：`0x40..0x99`
- 读长度检查：不能为 0，不能超过寄存器窗口，`reg + len - 1` 不能超过 `0x99`
- 临时读缓存从 255 字节缩小为实际最大寄存器窗口长度

非法访问会立即返回失败并置 `sys_time.crc_err`。

## 5. `FLAG2` 周期读取会自动清转换标志

### 问题是什么

PDF 说明读取 `FLAG2` 后，`VADC_FLG` 和 `CADC_FLG` 会自动清零。主工程每个采样周期都会读 `0x58..0x5C`，其中包括 `FLAG2`。

### 影响

如果以后其他模块也想通过 `FLAG2.VADC_FLG/CADC_FLG` 判断“是否有新 ADC 数据”，它可能已经被采样模块读掉并清零，造成事件丢失。

### 本轮修复

这轮没有停止读取 `FLAG2`，因为保护状态也在 `FLAG2` 里，周期读取仍然必要。处理方式是明确所有权：

- `UpdateVoltageFromBqMaximo()` 作为唯一周期读取方。
- 读取成功后把转换完成标志锁存到 `AFE1_LastFlag2ConversionFlags`。
- 以后其他模块如果需要转换完成状态，应读这个锁存值，而不是再次直接读 `FLAG2`。

## 6. 仍需实物验证

本轮是代码层修复，还需要通过 Keil/ST-Link 验证：

- 冷启动后 `SCONF4` 是否等于 EEPROM/运行期串数。
- `0x55..0x57` 读回镜像是否与 AFE 实际寄存器顺序一致。
- 初始化阶段写后读回是否稳定，是否有特殊寄存器会自清导致误判。
- 非法 SPI 地址调用是否能直接失败，不影响后续正常采样。
- 采样周期中 `AFE1_LastFlag2ConversionFlags` 是否能捕获 VADC/CADC 完成标志。
