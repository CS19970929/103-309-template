# 通信写 EEPROM 标志位映射表

本文是 `COMMUNICATION_EEPROM_FLAG_REFACTOR_DEBUG.md` 的配套表，目的很单一：

- EEPROM 地址不变
- 通信寄存器地址不变
- 只把当前过多的写标志位，收敛成更少的块级 dirty 位
- 方便你在 Keil 在线调试时逐项核对

## 1. 建议的新标志结构

### 1.1 总 dirty 位图

建议最终只保留一个总位图：

- `u32EepromDirtyMask`

每一位表示一个大块：

| 位号 | 块名 | 说明 |
|---|---|---|
| 0 | `CALIB` | K/B 校准块 |
| 1 | `PROTECT` | 保护参数块 |
| 2 | `RTC` | RTC 块 |
| 3 | `SOC_TABLE` | SOC 表块 |
| 4 | `COPPERLOSS` | 铜损块 |
| 5 | `OTHER1` | OtherElement1 块 |
| 6 | `HEAT_COOL` | 热管理块 |
| 7 | `PRODUCT_INFO` | 产品信息块 |
| 8 | `EVENT_RECORD` | 事件记录块 |
| 9 | `AFE_PARAM` | AFE 参数块 |
| 10 | `OFFSET` | 当前偏移量块 |
| 11 | `SYS_FLAG` | 系统功能 / 开关状态块 |

### 1.2 建议保留的少量子 mask

只给大块保留子 mask：

- `u32ProtectDirtyMask`
- `u32CalibDirtyMask`
- `u32Other1DirtyMask`
- `u32HeatCoolDirtyMask`

其他块如果一次就是整块更新，可以只保留块级 dirty，不再细分。

## 2. 旧标志到新标志的映射

### 2.1 校准相关

| 旧标志 | 当前来源 | 建议新标志 |
|---|---|---|
| `u8E2P_KB_WriteFlag` | `Sci_WrRegs_0x10_CalibCoef()` / `EEPROM_ResetData_AllToDefault()` | `u32EepromDirtyMask.bit0` + `u32CalibDirtyMask` |

说明：

- 现在 `KB_NUM` 是逐对写
- 未来可继续按 `KB index` 分段写
- 对 Keil 调试最友好的是看 `u32CalibDirtyMask` 是否正确减位

### 2.2 保护参数

| 旧标志 | 当前来源 | 建议新标志 |
|---|---|---|
| `u32E2P_Pro_VolCur_WriteFlag` | `Sci_WrRegs_0x10_Protect()`、`Sci_WrReg_0x06_Reset_ProtectElement()` | `u32EepromDirtyMask.bit1` + `u32ProtectDirtyMask` |
| `u32E2P_Pro_Temp_WriteFlag` | 同上 | `u32EepromDirtyMask.bit1` + `u32ProtectDirtyMask` |
| `u32E2P_Pro_Other_WriteFlag` | 同上 | `u32EepromDirtyMask.bit1` + `u32ProtectDirtyMask` |

建议的子区分：

| 子块 | 当前范围 | 建议子位范围 |
|---|---|---|
| 电压 / 电流保护 | `0x2000 ~ 0x201D` 一类 | 0..29 |
| 温度保护 | `0x201E ~ 0x2036` 一类 | 30..54 |
| 其他保护 | `0x2037 ~ 0x2040` 一类 | 55..64 |

备注：

- 当前代码里 3 个写标志实际上只是为了分段写
- 从调试角度看，它们完全可以并成一个保护块 dirty

### 2.3 RTC

| 旧标志 | 当前来源 | 建议新标志 |
|---|---|---|
| `u32E2P_RTC_Element_WriteFlag` | `Sci_WrRegs_0x10_RTC()` | `u32EepromDirtyMask.bit2` |

说明：

- RTC 块本来就是整块更新
- 没必要再拆更细

### 2.4 SOC 表

| 旧标志 | 当前来源 | 建议新标志 |
|---|---|---|
| `u8E2P_SocTable_WriteFlag` | `Sci_WrRegs_0x10_SocTable()` | `u32EepromDirtyMask.bit3` |

说明：

- 这是典型整块写入
- 直接一个块级 dirty 即可

### 2.5 铜损

| 旧标志 | 当前来源 | 建议新标志 |
|---|---|---|
| `u8E2P_CopperLoss_WriteFlag` | `Sci_WrRegs_0x10_CopperLoss()` | `u32EepromDirtyMask.bit4` |

说明：

- 也是整块写入
- 建议不再拆字段级 flag

### 2.6 OtherElement1

| 旧标志 | 当前来源 | 建议新标志 |
|---|---|---|
| `u32E2P_OtherElement1_WriteFlag` | `Sci_WrRegs_0x10_Balance()`、`Sci_WrRegs_0x10_SysOther()`、`Sci_WrRegs_0x10_SleepElement()`、`Sci_WrRegs_0x10_SocElement()`、`Sci_WrRegs_0x10_SystemElement()` | `u32EepromDirtyMask.bit5` + `u32Other1DirtyMask` |

建议的子位：

| 子位 | 对应功能 |
|---|---|
| 0..3 | 平衡参数 |
| 4..7 | 打开时间 / MOS 相关 |
| 8..11 | CS / CBC / 电流模式 |
| 12..15 | 冷却相关 |
| 16..23 | 睡眠相关 |
| 24..27 | SOC 相关 |
| 28..31 | 系统串数 / 预充 / 电阻相关 |

备注：

- 当前代码里这个标志集合是最乱的一块
- 最适合先改成“块级 dirty + 子 mask”

### 2.7 热管理

| 旧标志 | 当前来源 | 建议新标志 |
|---|---|---|
| `u32E2P_HeatCool_WriteFlag` | `Sci_WrRegs_0x10_HeatCoolElement()`、`Sci_WrReg_0x06_Reset_HeatCool()` | `u32EepromDirtyMask.bit6` + `u32HeatCoolDirtyMask` |

说明：

- 建议保留子 mask
- 但不再让通信层去逐位理解 EEPROM 写顺序

### 2.8 产品信息

| 旧标志 | 当前来源 | 建议新标志 |
|---|---|---|
| `ProductionInfor.BMS_SerialNumber_WriteFlag` | `Sci_WrRegs_0x10_SN_Version()` | `u32EepromDirtyMask.bit7` |
| `ProductionInfor.BMS_HardWareVersion_WriteFlag` | `Sci_WrRegs_0x10_SN_Version()` | `u32EepromDirtyMask.bit7` |
| `ProductionInfor.BMS_SoftWareVersion_WriteFlag` | `Sci_WrRegs_0x10_SN_Version()` | `u32EepromDirtyMask.bit7` |

说明：

- 这三项逻辑上属于同一块
- 没必要各自维护一套写状态

### 2.9 事件记录

| 旧标志 | 当前来源 | 建议新标志 |
|---|---|---|
| `gu8_Reset_EventRecord` | `Sci_WrReg_0x06_Reset_EventRecord()` | `u32EepromDirtyMask.bit8` 或独立 `event_clear_pending` |

说明：

- 事件清空比较特殊
- 可以保留一个独立的 `event_clear_pending`
- 但不要和普通参数写标志混在一起

### 2.10 AFE 参数

| 旧标志 | 当前来源 | 建议新标志 |
|---|---|---|
| `AFE_PARAM_WRITE_Flag` | `Sci_WrRegs_0x10_SysOther()`、`Sci_WrRegs_0x10_SystemElement()`、`Sci_WrRegs_0x10_FlashConnect()` 相关流程 | `u32EepromDirtyMask.bit9` |

说明：

- AFE 参数通常是联动写
- 对调试来说，最好把它当成一个独立块

### 2.11 偏移量

| 旧标志 | 当前来源 | 建议新标志 |
|---|---|---|
| `curr_offset` / `OffsetValue_CHG` / `OffsetValue_DSG` | `DataLoad_CurrentCali_startup()`、`InitData_E2prom()` | `u32EepromDirtyMask.bit10` |

说明：

- 这是运行相关持久化值
- 不建议再和通信参数混写在一起理解

### 2.12 系统功能位

| 旧标志 | 当前来源 | 建议新标志 |
|---|---|---|
| `System_OnOFF_Func` 相关位 | `Sci_WrReg_0x06_SwitchON()`、`Sci_WrReg_0x06_SwitchOFF()`、`Sci_WrReg_0x06_BMS_FunctionON()`、`Sci_WrReg_0x06_BMS_FunctionOFF()` | `u32EepromDirtyMask.bit11` |

说明：

- 这是运行状态和 EEPROM 持久化状态的交界区
- 最好单独一个块来跟踪

## 3. 旧标志如何减少

建议的缩减方式如下：

### 3.1 从多变量变成一个总掩码

把这些变量逐步合并：

- `u8E2P_KB_WriteFlag`
- `u32E2P_Pro_VolCur_WriteFlag`
- `u32E2P_Pro_Temp_WriteFlag`
- `u32E2P_Pro_Other_WriteFlag`
- `u32E2P_OtherElement1_WriteFlag`
- `u32E2P_HeatCool_WriteFlag`
- `u32E2P_RTC_Element_WriteFlag`
- `u8E2P_SocTable_WriteFlag`
- `u8E2P_CopperLoss_WriteFlag`

合并后先保留一个：

- `u32EepromDirtyMask`

### 3.2 再按需要保留少量子位图

只保留：

- `u32CalibDirtyMask`
- `u32ProtectDirtyMask`
- `u32Other1DirtyMask`
- `u32HeatCoolDirtyMask`

这样 Keil 里看变量时非常直观：

- `u32EepromDirtyMask` 看有没有任务
- 子 mask 看具体是哪个字段组

## 4. Keil 调试时建议看的变量

### 4.1 先看总状态

- `u32EepromDirtyMask`
- `System_ErrFlag.u8ErrFlag_Com_EEPROM`
- `MCUO_E2PR_WP`

### 4.2 再看块级状态

- `u32ProtectDirtyMask`
- `u32CalibDirtyMask`
- `u32Other1DirtyMask`
- `u32HeatCoolDirtyMask`

### 4.3 再看当前业务变量

- `g_u16CalibCoefK[]`
- `g_i16CalibCoefB[]`
- `PRT_E2ROMParas`
- `OtherElement`
- `Heat_Cool_Element`
- `ProductionInfor`
- `RTC_time`

### 4.4 断点推荐

按这个顺序最好调：

1. `Sci_WrRegs_0x10_*()`
2. `Sci_WrReg_0x06_*()`
3. `App_E2promDeal()`
4. `EEPROM_Process()` 以后如果你重构出来的话
5. `WriteEEPROM_ByteData_Circle()`
6. `WriteEEPROM_Word_NoZone()`
7. `WriteEEPROM_Byte()`

## 5. 实施建议

### 第一阶段

不删旧 flag，只新增：

- `u32EepromDirtyMask`
- `u32ProtectDirtyMask`
- `u32Other1DirtyMask`
- `u32HeatCoolDirtyMask`

然后在通信写入后同时置新旧标志，方便对照。

### 第二阶段

确认新流程稳定后：

- 先删 `u8E2P_SocTable_WriteFlag`
- 再删 `u8E2P_CopperLoss_WriteFlag`
- 再删 `u32E2P_RTC_Element_WriteFlag`
- 再收 `u32E2P_Pro_*` 三个保护标志

### 第三阶段

最终只保留块级 dirty 和少量子 mask。

## 6. 结论

最适合你这个工程的收敛方式不是“完全重写”，而是：

- 地址和协议保持不动
- 标志位从字段级改成块级
- 少数大块保留子 mask
- 调试时优先看 dirty、WP、回读结果和错误标志

这会明显降低通信写 EEPROM 的复杂度，也更适合 Keil 在线观察。
