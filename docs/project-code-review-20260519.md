# 主工程完整 Code Review 报告

日期：2026-05-19

范围：

- 主工程：`103 + 309`
- AFE 文档：`E:/nas sync win/work/SH36735XX CV0.2C.pdf`
- 重点模块：AFE SPI/寄存器、采样链路、保护/MOS、SOC、休眠、EEPROM/Flash 参数、Keil 工程配置

## 1. 总结

当前工程可以通过 Keil 重编译，但存在几类需要优先处理的量产风险：

1. MCU 型号/Flash 地址配置不一致，可能导致 Flash 越界访问。
2. 当前工作树打开了调试电流宏并关闭 watchdog，不能直接作为量产配置。
3. 上位机写入系统参数时缺少限幅，可能触发除 0 或数组越界。
4. AFE 采样失败只报 SPI 错误，没有进入 MOS/保护的失效安全路径。
5. AFE 保护配置硬编码，与 EEPROM/上位机参数脱节，且当前关闭了 TS4 温度保护。
6. 电流、电压、温度换算存在下溢或除 0 风险，会污染 SOC 和保护输入。

SOC 自耗补偿本身目前隔离较好：只在静置路径运行，不参与主充放电积分、循环次数和满空端点校准。真正会破坏 SOC 体验的风险主要来自虚拟电流宏、旧采样数据复用和电流校准下溢。

## 2. 构建验证

命令：

```text
C:\Keil_v5\UV4\UV4.exe -j0 -r "E:\TODO\103-309-template\103 + 309\Project\Users\CommomSH367309_16series_103RCT6_C.uvprojx" -t "Target 1"
```

结果：

- 工程：`CommomSH367309_16series_103RCT6_C.uvprojx`
- Target：`Target 1`
- 器件配置：`STM32F103C8`
- 工具链：ARMCC V5.06 update 7 build 960
- 编译结果：`0 Error(s), 52 Warning(s)`
- Program Size：`Code=54192 RO-data=2372 RW-data=1256 ZI-data=6040`
- ROM：`56780 bytes`
- RAM：`7296 bytes`
- 产物：
  - `103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.axf`
  - `103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.bin`

需要注意：52 个 warning 中包含隐式函数声明、非 void 函数缺 return、枚举混用、无意义 unsigned 比较。这些虽然未阻断构建，但不应长期保留在保护板量产工程里。

## 3. 高优先级问题

### P0-1 MCU 型号与 Flash 地址不一致

位置：

- `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`
- `103 + 309/Project/Source/Flash.h`

现象：

- Keil 工程配置为 `STM32F103C8`，IROM 为 `0x08000000..0x0800FFFF`。
- `Flash.h` 使用 `0x0801F000`、`0x0801F400`、`0x0801F800`、`0x0801FC00` 保存 AFE 校准、升级、休眠标志。

风险：

- 如果实际芯片真是 C8，这些地址超出 64KB Flash 范围，会导致 Flash 操作失败或不可预期行为。
- 如果实际硬件是 RCT6，则工程目标、IROM 区间和芯片型号配置错误，构建产物边界不可信。

建议：

- 先确认实际 BOM MCU 型号。
- 若为 RCT6，修正 Keil Device 与 IROM。
- 若为 C8，必须重新规划持久化地址，不能使用 `0x0801Fxxx`。

### P0-2 当前工作树存在调试配置

位置：

- `103 + 309/Project/Source/conf/conf.h`
- `103 + 309/Project/Source/DataDeal.c`

现象：

- `wdog_enable` 当前被注释。
- `__VIRTURE_CURRENT__` 当前打开。
- `DataLoad_Current()` 在 `sys_time.isdebugenable == 1` 时用 `sys_time.CHG/DSG` 覆盖真实充放电电流。

风险：

- 关闭 watchdog 会降低异常自恢复能力。
- 虚拟电流会污染 SOC、休眠、保护、MOS 控制和时间估算。

建议：

- 建议建立量产配置宏检查，例如 `PRODUCTION_BUILD` 下禁止 `__VIRTURE_CURRENT__`，强制启用 watchdog。
- 虚拟电流调试能力保留可以，但应默认关闭，并且只允许在明确 debug build 下编译。

### P0-3 上位机写系统参数缺少限幅和除 0 保护

位置：

- `103 + 309/Project/Source/Sci_Upper.c`
- `103 + 309/Project/Source/main.c`
- `103 + 309/Project/Source/DataDeal.c`

现象：

- `Sci_WrRegs_0x10_SystemElement()` 直接写 `OtherElement.u16Sys_SeriesNum`、`u16Sys_CS_Res`。
- 之后直接 `SeriesNum = OtherElement.u16Sys_SeriesNum`。
- `g_u32CS_Res_AFE = OtherElement.u16Sys_CS_Res_Num * 1000 / OtherElement.u16Sys_CS_Res` 没有检查除数。
- `DataLoad_CellVolt()` 按 `SeriesNum` 读取 `SH367309_Read_AFE1.u16VCell[20]`。

风险：

- `u16Sys_CS_Res == 0` 会除 0。
- `SeriesNum > 20` 会越界读 AFE 电芯缓存。
- `SeriesNum > 32` 还可能越界访问上报单体数组。

建议：

- 所有入口统一调用串数规范化函数，限制到 AFE 支持范围。
- `CS_Res`、`CS_Res_Num` 增加最小值和最大值检查。
- 参数非法时拒绝 ACK 或回退默认值，并记录错误。

## 4. AFE 与保护链路问题

### P1-1 AFE 采样失败没有进入失效安全

位置：

- `103 + 309/Project/Source/I2C_AFE1.c`
- `103 + 309/Project/Source/DataDeal.c`
- `103 + 309/Project/Source/IO_Control.c`

现象：

- `UpdateVoltageFromBqMaximo()` 读失败只置 `ERROR_SPI`。
- `App_AFEGet()` 发现 AFE 更新失败后直接返回，不更新电压、电流、温度。
- `isforceClose()` 检查 `ERROR_AFE1/AFE2/EEPROM/CBC/temp`，不检查 SPI 错误。

风险：

- SPI/CRC 连续失败时，系统可能继续使用旧电压、旧电流和旧 MOS 状态。
- 均衡会因为 SPI 错误禁用，但 MOS 强关路径不一定触发。

建议：

- AFE 连续读失败达到阈值后，同步置 `ERROR_AFE1` 或单独增加 AFE 采样失效状态。
- MOS 控制强关条件纳入 SPI/AFE 数据失效。
- 失败时冻结 SOC 计算输入或明确标记采样无效，避免旧数据继续参与积分。

### P1-2 AFE 保护配置硬编码且关闭 TS4

位置：

- `103 + 309/Project/Source/main.c`

现象：

- `AFE3520_SCONF6_PROTECT_EN` 为 `0x7f`。
- 文档中 `SCONF6` bit7 是 `TS4_EN`，默认值为 1。
- 初始化写入 `0x7f` 会关闭 TS4 温度保护。
- OV/UV/OCD/OCC 阈值在初始化中硬编码，未与 EEPROM/上位机保护参数联动。

风险：

- 如果硬件接了 TS4，第四路温度硬件保护失效。
- 用户修改软件保护参数后，AFE 硬件保护阈值可能仍停留在硬编码值。

建议：

- 根据实际温度通道配置生成 SCONF6，不要固定 `0x7f`。
- AFE 硬件保护阈值应从同一套保护参数派生，并在参数变更后写入 AFE 且读回校验。

### P1-3 AFE 清保护标志逻辑可疑

位置：

- `103 + 309/Project/Source/SH367309_Func.c`
- `103 + 309/Project/Source/SH367309_Func.h`

现象：

- `SH_AFE_ClearProtectFlag()` 对 FLAG2 使用：

```c
Temp = Registers_AFE1.flag2.all & (uint8_t)(~(AFE_Protect | 0xFE));
```

风险：

- `AFE_Protect` 的 FLAG2 枚举带 `0x1000` 高位标记，当前表达式容易得到非预期掩码。
- 可能清错标志，或保留/覆盖不该动的 FLAG2 位。
- 写 `SCONF2.LTCLR` 的返回值未检查。

建议：

- 将寄存器选择和 bit mask 拆开处理，例如 `flag_mask = AFE_Protect & 0xff`。
- 每次清标志应检查 `LTCLR` 写入是否成功，并在写 FLAG 后读回确认。

### P1-4 休眠进入闭环不完整

位置：

- `103 + 309/Project/Source/SleepDeal.c`
- `103 + 309/Project/Source/SH367309_Func.c`

现象：

- 写休眠 Flash 标志成功后，直接 `AFE_Sleep()`，随后 MCU reset。
- `AFE_Sleep()` 只执行均衡关闭和 `SCONF1=0xAA` 写入，没有检查返回值或读回 `BSTATUS2.SLEEP`。

文档依据：

- SH36735XX 文档说明写 `SCONF1=0xAA` 后进入 SLEEP，AFE 会关闭 CADC/WDT、电压电流保护、MOS、电荷泵和均衡。

风险：

- 如果写 SLEEP 失败，MCU 仍复位，系统对睡眠状态的判断会和 AFE 实际状态不一致。

建议：

- 进入睡眠前写 `SCONF1=0xAA` 后读回状态。
- 失败时不要立即复位，记录错误并保持可恢复状态。

## 5. 采样与计算问题

### P1-5 电流/电压校准存在无符号下溢

位置：

- `103 + 309/Project/Source/DataDeal.c`

现象：

- 总压计算中负 B 值被转为 `UINT32` 后参与计算。
- 充放电电流校准中，负 B 值加到 `UINT32` 电流累计上。
- 后续 `u32_xxx > 0 ?` 对无符号类型没有实际负值保护意义。

风险：

- 负校准值幅度大于当前采样时，会下溢成很大的正数。
- SOC、保护、休眠和通信显示都会被异常电流或电压污染。

建议：

- 使用 `INT32` 或 `int64_t` 做中间计算。
- 校准后统一限幅到 `0..UINT16_MAX` 或业务允许范围。

### P1-6 温度换算可能除 0 或下溢

位置：

- `103 + 309/Project/Source/I2C_AFE1.c`

现象：

```c
u32temp = 1000 * raw / (32768 - raw);
```

风险：

- 当 `raw >= 32768` 时，分母为 0 或无符号下溢。
- 异常采样、断线、SPI 旧数据都可能触发。

建议：

- 温度 ADC 原始值进入换算前先判断范围。
- 异常值应标记温度断线/采样无效，不进入查表。

### P2-1 放电电流计算顺序导致精度不一致

位置：

- `103 + 309/Project/Source/DataDeal.c`

现象：

- 充电电流使用 `raw * 100 * res / 29127`。
- 放电电流使用 `raw * res / 29127 * 100`。

风险：

- 放电路径先除后乘，低电流时精度损失更明显。

建议：

- 统一计算顺序，优先用 64 位中间值。

## 6. SOC 专项结论

当前 SOC 自耗补偿设计符合“自耗不影响其他 SOC 计算、校准逻辑”的目标：

- 自耗只在无充电、无放电分支执行。
- 使用独立 `s_u32SelfConsumeMaMs`、`s_u32SelfConsumeCapChange`。
- 不写主积分 `u32CapChange`。
- 不累计 `u8DSG_SOC_Int` 和 `u32Cycle_times`。
- 方向切换、外部重设 SOC、满电/空电锚定会清零自耗余量。

仍需注意：

- `__VIRTURE_CURRENT__` 打开时，SOC 输入电流可能被调试值覆盖。
- AFE 采样失败后旧电流继续保留，SOC 可能继续沿旧方向积分或误判静置。
- 电流校准下溢会直接导致 SOC 大幅跳变。

建议 SOC 修复优先级：

1. 先禁用量产虚拟电流。
2. 采样失败时冻结 SOC 输入或标记无效。
3. 修复电流校准有符号中间值。
4. 增加 SOC 回放测试：充电到满、放电到空、静置 OCV、自耗 15mA、AFE 采样失败。

## 7. 构建 Warning 中应优先处理的项

优先处理这些不是纯风格问题的 warning：

- `AFE_PROTECT_param.c`：`sh_decode_occ_occt()` 缺 return。
- `SH367309_DataDeal.c`：`fac_sh367309_param_init_first_powerup()` 非 void 函数缺 return。
- `rtc_sleep.c`：多个隐式函数声明，包括 `isHaveCurrent_sh3x()`、`DataLoad_Current()`、`DataLoad_CellVolt()`、`Init()`、`LogEvent_Record()`、`SleepDeal_Continue()`。
- `rtc_sleep.c`：`update_rtc_soc()` 在当前编译路径存在缺 return。
- `EEPROM.c`：对 `FLASH_ADDR_SH367309_VALUE` 通过 `WriteEEPROM_Word_NoZone()` 写入触发截断 warning，本质是把 Flash 地址传给 16 位 EEPROM 地址接口。

## 8. 建议修复顺序

1. 确认实际 MCU 型号，并修正 Keil Device/IROM/Flash 持久化地址。
2. 建立量产宏检查，禁止 watchdog 关闭和虚拟电流开启进入量产构建。
3. 对上位机写入参数做统一限幅，尤其是串数和采样电阻。
4. AFE 采样失败接入失效安全策略，连续失败后强关 MOS 或进入受控故障。
5. 修复电流/电压/温度换算的下溢、除 0 和旧数据污染。
6. AFE 硬件保护阈值从参数生成，并补齐写后读回校验。
7. 清理高风险 warning，再逐步清理剩余 warning。
8. 补 SOC/AFE 主机回放测试和上板通信验证脚本。

## 9. 未验证项

本次未做上板验证，未通过 RS485/CAN 实测读写参数，也未用示波器复核 SPI 波形。AFE 文档对 SPI 写 CRC 的文字描述提到“写数据长度”，但时序图和已有专题文档中的官方例程均按 `cmd/reg/data` 三字节计算 CRC；当前工程也采用该方式。本项建议以后以官方例程和板上实测为准，不作为本次确定 bug。
