# AFE SPI 通信专项梳理

生成日期：2026-05-18  
主项目：`103 + 309`  
AFE 芯片：SH3673520  
官方参考：`SH3673520+STM32F072CBT6 DemoCode V1.2_20241227`

## 1. 结论

当前主项目和 AFE 芯片的 SPI 通信存在明确问题，不能认为是可靠闭环。

更准确地说：

- 物理层配置方向大体对：官方例程使用 SPI Mode 3，主项目软件 SPI 也按 Mode 3 实现，SCK 空闲高电平、第二边沿采样。
- CRC 算法方向大体对：主项目 CRC8 算法和官方表法一致，都是 poly `0x07`、init `0x00`。
- 协议层存在确定 bug：写寄存器 ACK 读取位置错误，软件复位少发最后一个 dummy 字节，读寄存器没有完整校验回显字段。
- 上层错误处理不完整：采样函数忽略所有 SPI 读返回值，初始化写寄存器不检查返回值，也没有官方例程的重试、寄存器读回校验和 SPI 错误闭环。
- CS 初始状态有风险：`InitIO()` 只把 CS 配成输出，没有拉高；当前软件 SPI 初始化也没有拉高 CS，第一次交易前 CS 可能长期保持低电平。

所以当前最危险的不是“完全不通”，而是“可能能读到部分数据、也可能实际写进去了，但软件不知道成功失败，并且失败时仍继续使用旧数据或错误数据”。

## 2. 先理解 SPI

SPI 是一种同步串行总线，主项目里 MCU 是主机，SH3673520 是从机。

四根主要线：

- `CS` / `NSS`：片选。低电平表示选中 AFE，一帧通信开始；高电平表示一帧结束。
- `SCK`：时钟。主机输出，所有数据位都跟随时钟传输。
- `MOSI`：Master Out Slave In，MCU 发给 AFE。
- `MISO`：Master In Slave Out，AFE 回给 MCU。

SPI 的关键特点：

- SPI 是全双工。MCU 每发 1 个字节，同时也会从 MISO 收 1 个字节。
- 读数据时，MCU 也必须继续发 dummy 字节，例如 `0x00`，只是为了提供时钟，让 AFE 把数据吐出来。
- ACK 也是一样。AFE 的 ACK 不会自己“冒出来”，MCU 必须再发 1 个 dummy 字节，才能在 MISO 上读到 ACK。
- SPI Mode 决定 SCK 空闲电平和采样边沿。SH3673520 官方例程使用 Mode 3：`CPOL=1`、`CPHA=1`，也就是 SCK 空闲高电平，第二个边沿采样。
- CRC 是 SH3673520 协议层的 CRC，不是 STM32 SPI 外设硬件 CRC。官方例程里 `CRCCalculation` 也是 disable。

## 3. 官方例程的 SH3673520 SPI 帧格式

官方代码集中在：

- `BMS_Drivers/Src/SPIApp.c`
- `BMS_Drivers/Inc/SPIApp.h`
- `Core/Src/spi.c`

### 3.1 SPI 模式

官方 `MX_SPI1_Init()`：

- 主机模式。
- 2 线全双工。
- 8 bit。
- `CLKPolarity = SPI_POLARITY_HIGH`。
- `CLKPhase = SPI_PHASE_2EDGE`。
- `NSS = SPI_NSS_SOFT`。
- `BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64`。
- `FirstBit = SPI_FIRSTBIT_MSB`。
- 硬件 CRC 关闭。

这就是 Mode 3、MSB first、软件控制 CS。

### 3.2 写寄存器帧

官方写操作固定 5 字节：

```text
TX: [0] 0x01
TX: [1] RegAddr
TX: [2] Data
TX: [3] CRC8(0x01, RegAddr, Data)
TX: [4] 0x00

RX: [0] 0xFF
RX: [1] 0x01
RX: [2] RegAddr
RX: [3] Data
RX: [4] 0xA5
```

官方成功条件：

- `RX[0] == 0xFF`
- `RX[1] == 0x01`
- `RX[2] == RegAddr`
- `RX[3] == Data`
- `RX[4] == 0xA5`

重点：ACK 在第 5 个接收字节 `RX[4]`，需要 TX 最后一字节 `0x00` 来把 ACK 时钟出来。

### 3.3 读寄存器帧

官方读操作发送 `RdLen + 5` 字节：

```text
TX: [0] 0x02
TX: [1] RegAddr
TX: [2] RdLen
TX: [3..RdLen+4] 0x00

RX: [0] 0xFF
RX: [1] 0x02
RX: [2] RegAddr
RX: [3] RdLen
RX: [4..RdLen+3] Data
RX: [RdLen+4] CRC8(RX[0..RdLen+3])
```

官方成功条件：

- 前 4 个回显字段都正确。
- 最后 1 字节 CRC 正确。
- 读出的数据从 `RX[4]` 开始。

### 3.4 软件复位帧

官方软件复位也是 5 字节：

```text
TX: [0] 0x0B
TX: [1] 0xBB
TX: [2] 0xCC
TX: [3] CRC8(0x0B, 0xBB, 0xCC)
TX: [4] 0x00

RX: [0] 0xFF
RX: [1] 0x0B
RX: [2] 0xBB
RX: [3] 0xCC
RX: [4] 0xA5
```

### 3.5 官方错误处理

官方不是只做一次读写：

- `SH_AFE_Write()` 写失败会最多重试 5 次。
- `SH_AFE_Read()` 读失败会最多重试 5 次。
- `bAfeSPIRWErrFlg` 记录最近 SPI 读写是否失败。
- `SH_AFE_SPICheck()` 连续检测到 SPI 异常后置 `AFE_ERR`。
- `SH_AFE_RegisterInit()` 初始化寄存器时检查每次写是否成功。
- `SH_AFE_RegisterCheck()` 周期性读回关键寄存器，确认没有被改写。
- `SH_AFE_ReadRegister()` 返回错误 bitmask，区分 FLAG、STATUS、CELL、TEMP、CURRENT 等不同读失败。

## 4. 主项目当前 SPI 通信路径

### 4.1 实际参与构建的文件

Keil 工程中：

- `sh36735_spi_proto.c` 参与构建。
- `sh36735_spi_sw.c` 参与构建。
- `sh36735_spi_hw.c` 存在，但 `IncludeInBuild=0`，当前不参与构建。
- `bsp_spi_bus.c` 参与构建，主要负责 GPIO 配置。

所以当前实际使用的是 GPIO 模拟 SPI，而不是 STM32 硬件 SPI。

### 4.2 引脚

来自 `conf_gpio.h` 和 `sh36735_port.h`：

- `CS`：PA4
- `SCK`：PA5
- `MISO`：PA6
- `MOSI`：PA7

### 4.3 初始化顺序

`main.c` 当前初始化顺序里和 SPI 相关的是：

```text
InitIO()
...
bsp_InitSPIBus()
sh36735_spi_sw_init()
sh36735_read_regs(0x6B, ...)
sh36735_write_reg_u8(...)
```

`InitIO()` 会把 PA4 配成输出，但没有显式拉高 CS。`bsp_InitSPIBus()` 配置 PA5/PA7 输出、PA6 输入。`sh36735_spi_sw_init()` 只把 SCK 和 MOSI 拉高，没有把 CS 拉高。

这意味着第一次调用 `sh36735_read_regs()` 前，CS 可能已经处于低电平，不一定有一个干净的“CS 高到低”起始边沿。

### 4.4 软件 SPI 时序

`sh36735_spi_sw.c`：

- SCK 空闲高。
- 发送每 bit 前先设置 MOSI。
- SCK 拉低。
- 延时。
- SCK 拉高。
- 延时。
- 读取 MISO。
- MSB first。

这符合 Mode 3 的基本语义。以 `sh_delay_us(1)` 粗略估算，SCK 约 500kHz，和官方硬件 SPI 分频后的量级接近。

所以单看 bit-bang 时序，不是最主要问题。

## 5. 已确认的问题

### P0-1：写寄存器 ACK 读取位置错误

主项目 `sh36735_write_reg_u8()` 当前逻辑：

```c
(void)sh36735_spi_xfer(0x01);
(void)sh36735_spi_xfer(reg);
(void)sh36735_spi_xfer(val);
uint8_t ack = sh36735_spi_xfer(crc);
sh36735_spi_xfer(0x00);
return (ack == 0xA5);
```

按官方帧格式，`sh36735_spi_xfer(crc)` 同时收到的是 `RX[3]`，也就是写入数据的回显，不是 ACK。真正 ACK 是下一次 `sh36735_spi_xfer(0x00)` 的返回值。

正确判断应该类似：

```text
rx0 = xfer(0x01)
rx1 = xfer(reg)
rx2 = xfer(val)
rx3 = xfer(crc)
rx4 = xfer(0x00)

成功 = rx0==0xFF && rx1==0x01 && rx2==reg && rx3==val && rx4==0xA5
```

当前后果：

- AFE 实际可能已经写成功，因为 5 个 TX 字节发出去了。
- 但函数返回值大概率是 false，因为它把数据回显当 ACK。
- 如果写入的数据刚好是 `0xA5`，还可能误判 true。
- 所有依赖 `sh36735_write_reg_u8()` 返回值的逻辑都不可信。

影响范围：

- AFE 初始化寄存器写入。
- MOS 开关控制。
- 硬件保护标志清除。
- 均衡寄存器写入。
- SCI 命令里直接写 AFE 寄存器的路径。

### P0-2：软件复位帧少发最后一个 dummy 字节

主项目 `sh36735_sw_reset()` 当前发：

```text
0x0B, 0xBB, 0xCC, CRC
```

官方要求：

```text
0x0B, 0xBB, 0xCC, CRC, 0x00
```

主项目不仅 ACK 位置读错，而且没有发送最后的 `0x00` 去 clock 出 ACK。这个函数当前基本不能作为可靠的软件复位判断。

### P0-3：初始化第一次 SPI 读存在指针截断 bug

`main.c` 中：

```c
sh36735_read_regs(0x6B, (uint8_t)&g_stCellInfoReport.u16VCell[1], 2);
```

这里把地址强转成了 `uint8_t`，不是 `uint8_t *`。这会把 32bit 地址截断成 8bit，再传给需要指针的参数。Keil 已经能给出相关 warning。

正确形式应是：

```c
sh36735_read_regs(0x6B, (uint8_t *)&g_stCellInfoReport.u16VCell[1], 2);
```

这行本身像是上电测试读，但它在初始化路径里真实执行，属于明确 bug。

### P0-4：采样函数忽略所有 SPI 读返回值

`UpdateVoltageFromBqMaximo()` 连续读取：

- `0x40..0x46`
- `0x47..0x57`
- `0x58..0x5C`
- `0x5D..0x96`

但所有 `sh36735_read_regs()` 的返回值都被忽略，函数最后固定返回 `result`，而 `result` 初始为 0，没有根据失败置位。

`App_AFEGet()` 又直接调用：

```c
UpdateVoltageFromBqMaximo();
DataLoad_CellVolt();
DataLoad_Current();
DataLoad_Temperature();
```

也就是说：

- 读失败时仍继续换算电压、电流、温度。
- 可能使用上一次旧数据。
- 可能使用结构体里部分更新、部分未更新的数据。
- 保护、MOS、SOC、通讯上报都会继续消费这些数据。

### P0-5：读寄存器没有完整校验回显字段

主项目 `sh36735_read_regs()` 当前只保存：

- `rx0`
- 数据区
- `crc_rx`

但忽略了：

- `RX[1]` 命令回显
- `RX[2]` 地址回显
- `RX[3]` 长度回显

CRC 计算时使用的是期望值：

```text
rx0, 0x02, reg, n, data...
```

官方则是对真实接收到的 `AFERxBuf[0..RdLen+3]` 做 CRC。这导致主项目无法区分“回显字段错误”和“数据错误”，错误定位能力明显弱于官方。

### P1-1：CS 初始状态不可靠

当前只有 `sh36735_spi_hw_init()` 会在 GPIO 初始化后调用 `sh_cs_high()`，但硬件 SPI 文件没有参与构建。

实际启用的软件 SPI 初始化 `sh36735_spi_sw_init()` 只做：

```c
sck_high();
mosi_high();
```

没有 `sh_cs_high()`。

而 `InitIO()` 初始化 CS 输出时没有先设置输出数据寄存器为高。STM32 GPIO 输出寄存器复位默认通常为 0，所以 PA4 配成输出后可能直接是低电平。对于 SPI 从机来说，CS 长时间低电平会让它认为一帧可能已经开始，第一次通信容易错帧。

建议在 SPI 初始化时强制：

```c
sh_cs_high();
sh_delay_us(...);
sck_high();
mosi_high();
```

并且每一帧都保证：

```text
CS high idle -> CS low -> 全帧字节 -> CS high -> 帧间隔
```

### P1-2：没有官方式重试包装

官方 `SH_AFE_Read()`、`SH_AFE_Write()` 最多重试 5 次。主项目底层 `sh36735_read_regs()`、`sh36735_write_reg_u8()` 只执行一次。

上层大部分调用也没有统一重试策略。

### P1-3：AFE 初始化没有读回校验

`main.c` 直接写一批 AFE 寄存器：

- `SCONF1`
- `SCONF2`
- `SCONF3`
- `SCONF4`
- `SCONF6`
- OV/UV/OCD/OCC/温度保护阈值

但没有检查写返回值，也没有读回确认。即使某个寄存器没写进去，系统也会继续启动。

官方做法是：

- `SH_AFE_RegisterInit()` 写每个寄存器时检查结果。
- `SH_AFE_RegisterCheck()` 周期性读回关键 RAM 寄存器，确认配置没有丢失或被异常改写。

### P1-4：SPI 错误没有进入系统错误闭环

主项目已有：

- `ERROR_SPI`
- `ERROR_REMOVE_SPI`
- `System_ErrFlag.u8ErrFlag_Com_SPI`
- `sys_time.crc_err`
- `sys_time.crc_err_cnt`

但当前实际链路没有完整打通：

- `MonitorAFE()` 里有旧 AFE 错误处理框架，但函数内部 `#if 0`，主要逻辑被关闭。
- `App_AFEGet()` 中 `MonitorAFE(0, UpdateVoltageFromBqMaximo())` 被注释。
- `sh36735_read_regs()` 只维护 `sys_time.crc_err` 一个布尔值，连续多次读取时后一次成功会覆盖前一次失败。
- `SleepDeal.c` 会在 `sys_time.crc_err` 持续 60 秒后深睡，但这不是完整的通讯故障处理。

### P1-5：均衡写入逻辑和 SPI 返回值处理错误

`Cell_balance.c` 中 `CB_AfeWriteBalanceMaskU24()`：

- 原本有 3 次重试和返回值判断，但被注释。
- 当前直接写三个均衡寄存器。
- 然后固定 `return 1`。

而 `CB_ApplyBalanceMask()` 把非 0 当失败：

```c
if (0 != CB_AfeWriteBalanceMaskU24(...))
{
    return 1;
}
```

这会造成：

- AFE 均衡寄存器可能已经被写了。
- 软件状态却认为应用失败，不更新 `s_u16BalanceActiveMask` 和上报状态。
- 如果底层写 ACK 判断又是错的，均衡状态更难确认。

### P2-1：软件 SPI 可用于调试，但生产可靠性需要验证

`sh36735_spi_sw.c` 文件注释本身写明，该软件 SPI 主要用于硬件 SPI 跑不通时的示波器定位，量产建议用硬件 SPI。

软件 SPI 不是一定不能用，但需要确认：

- SCK 频率和占空比。
- CS setup/hold/high 时间。
- 中断打断 bit-bang 时序的影响。
- MOSI/MISO 上升下降沿裕量。
- GPIO 输出速度和板级走线干扰。

如果产品要量产，建议优先切回硬件 SPI，或者至少用逻辑分析仪证明软件 SPI 在最差负载和中断场景下稳定。

### P2-2：项目中有多套 SPI 代码，容易误用

当前源码中存在：

- `bsp_spi_bus.c/h`：老通用 SPI 总线代码。
- `sh3520 driver/sh36735_spi_*`：当前实际 AFE SPI 驱动。
- `100ask driver/sh3673520_spi.*`：实验模板，协议和 Mode 配置不符合当前官方例程，且未集成。
- `common/sh_spi_port.h`：抽象接口头文件，目前未被主路径使用。

尤其 `100ask driver/sh3673520_spi_cfg.h` 默认 `SPI_MODE0`、`HAS_CRC=0`，不能作为当前 SH3673520 官方协议参考。

## 6. 当前问题会表现成什么现象

可能出现的现象包括：

- 上电 AFE 初始化看似执行了，但软件无法确认寄存器是否真正写入。
- MOS 开关函数调用后，软件返回值不可信，MOS 实际状态要靠读回 `BSTATUS` 或外部测量确认。
- 硬件保护标志清除函数可能总是返回 false，即使 AFE 实际已经收到写帧。
- 采样偶发 CRC 失败时，系统仍用旧电压、电流、温度继续跑保护和 SOC。
- 读回数据偶发错帧时，故障不会进入 `System_ErrFlag.u8ErrFlag_Com_SPI` 的正式错误路径。
- 均衡寄存器可能被写入，但软件状态显示未成功，或者反复尝试。
- 第一次 SPI 通信容易因为 CS 初始低电平和指针截断 bug 出现异常。

## 7. 建议验证方法

### 7.1 逻辑分析仪抓写寄存器

抓 PA4/PA5/PA6/PA7：

- PA4：CS
- PA5：SCK
- PA6：MISO
- PA7：MOSI

以写 `SCONF4` 为例，期望：

```text
CS: 先高，拉低开始，5 字节结束后拉高
SCK: 空闲高，Mode 3
MOSI: 0x01, 0x43, Data, CRC, 0x00
MISO: 0xFF, 0x01, 0x43, Data, 0xA5
```

如果 MISO 第 5 字节不是 `0xA5`，要查：

- CRC 是否正确。
- SPI Mode 是否正确。
- CS 是否覆盖完整 5 字节。
- AFE 是否供电、唤醒、未处于异常低功耗状态。

### 7.2 逻辑分析仪抓读寄存器

读 `SCONF4` 或 `FLAG1`：

```text
MOSI: 0x02, RegAddr, Len, 0x00...
MISO: 0xFF, 0x02, RegAddr, Len, Data..., CRC
```

重点看：

- 前 4 个回显是否正确。
- 数据长度是否对。
- 最后 CRC 是否能按 `MISO[0..Len+3]` 算出来。

### 7.3 软件上做最小闭环测试

建议先不要直接跑完整 BMS 逻辑，而是做最小测试：

1. 初始化 SPI，并强制 CS 高。
2. 写 `SCONF4 = SeriesNum`。
3. 立刻读回 `SCONF4`。
4. 判断写函数返回、读函数返回、读回值是否一致。
5. 连续循环 1000 次，统计失败次数。

再做错误注入：

1. 故意写错 CRC，确认返回失败。
2. 故意读不存在或不合理长度，确认返回失败。
3. 断开 MISO 或拉低/拉高，确认错误计数能上升。

## 8. 建议修复顺序

### 第一优先级：修底层协议帧

1. 在 `sh36735_write_reg_u8()` 中保存 5 个 RX 字节，按官方条件判断。
2. 在 `sh36735_sw_reset()` 中补最后一个 dummy 字节，并按官方条件判断。
3. 在 `sh36735_read_regs()` 中保存 `rx1/rx2/rx3`，校验 `0xFF/命令/地址/长度/CRC`。
4. SPI 初始化时强制 `CS=1`、`SCK=1`、`MOSI=1`，保证空闲态正确。

### 第二优先级：修上层错误传播

1. `UpdateVoltageFromBqMaximo()` 返回真实错误 bitmask，不要固定返回 0。
2. `App_AFEGet()` 读取失败时不要继续更新 `g_stCellInfoReport`，至少要保留旧值并置 SPI/AFE 错误。
3. 接入 `System_ERROR_UserCallback(ERROR_SPI)` 或专门的 AFE SPI 错误状态。
4. 避免 `sys_time.crc_err` 被同一轮后续成功读取覆盖，可改为本轮错误累计。

### 第三优先级：补读回和重试

1. 加官方式 5 次重试包装。
2. AFE 初始化后读回关键寄存器。
3. 周期性执行寄存器检查，发现配置异常就重写或进入故障。
4. MOS、均衡、保护清除等关键写操作必须读回确认。

### 第四优先级：整理 SPI 代码边界

1. 明确只保留一条 SH3673520 SPI 主路径。
2. `100ask driver` 标为实验代码或移出构建视野，避免误用。
3. 决定量产使用软件 SPI 还是硬件 SPI。如果使用硬件 SPI，需要启用 `sh36735_spi_hw.c` 并删除/排除软件 SPI 的同名 `sh36735_spi_xfer()`。
4. 把官方例程中的帧格式注释直接写到驱动头文件或文档里。

## 9. 建议的修复后判定标准

可以认为 AFE SPI 通信基本可靠，需要至少满足：

- 写寄存器时能抓到完整 5 字节，`RX[4] == 0xA5`。
- 读寄存器时能抓到完整 `Len+5` 字节，回显和 CRC 全部正确。
- 连续读关键寄存器 1000 次无 CRC 错误，或者错误率可解释且有重试恢复。
- 上电初始化失败会被记录并阻止继续使用未初始化 AFE 数据。
- 周期采样失败会进入错误路径，不再静默使用旧数据。
- MOS、均衡、保护清除这类关键写操作有返回值检查和读回确认。

