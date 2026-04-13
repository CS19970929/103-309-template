# 通信 EEPROM 标志位收敛与 Keil 调试方案

本文说明当前通信写 EEPROM 的收敛方案，目标是：
- EEPROM 地址不变
- 通信寄存器地址不变
- 写入逻辑更简单、更安全、更容易调试

## 1. 现状

当前通信写 EEPROM 的入口主要集中在 `Sci_Upper.c` 的这些函数：
- `Sci_WrRegs_0x10_CalibCoef()`
- `Sci_WrRegs_0x10_Protect()`
- `Sci_WrRegs_0x10_SocTable()`
- `Sci_WrRegs_0x10_CopperLoss()`
- `Sci_WrRegs_0x10_RTC()`
- `Sci_WrRegs_0x10_Balance()`
- `Sci_WrRegs_0x10_SysOther()`
- `Sci_WrRegs_0x10_SleepElement()`
- `Sci_WrRegs_0x10_SocElement()`
- `Sci_WrRegs_0x10_SystemElement()`
- `Sci_WrRegs_0x10_HeatCoolElement()`
- `Sci_WrRegs_0x10_SN_Version()`
- `Sci_WrReg_0x06_BMS_FunctionON()`
- `Sci_WrReg_0x06_BMS_FunctionOFF()`

问题不在于“有没有标志位”，而在于：
- 标志位太多
- 有些地方直接写 EEPROM
- 有些地方先改 RAM 再等后台写
- 有些块的后台分支曾经被注释

## 2. 当前收敛策略

### 2.1 保留一层总 dirty

新增：
- `u32EepromDirtyMask`

通过块级位来管理 EEPROM 是否需要落盘：
- `EEPROM_DIRTY_BLOCK_CALIB`
- `EEPROM_DIRTY_BLOCK_PROTECT`
- `EEPROM_DIRTY_BLOCK_RTC`
- `EEPROM_DIRTY_BLOCK_SOC_TABLE`
- `EEPROM_DIRTY_BLOCK_COPPERLOSS`
- `EEPROM_DIRTY_BLOCK_OTHER1`
- `EEPROM_DIRTY_BLOCK_HEAT_COOL`
- `EEPROM_DIRTY_BLOCK_PRODUCT_INFO`
- `EEPROM_DIRTY_BLOCK_EVENT_RECORD`
- `EEPROM_DIRTY_BLOCK_AFE_PARAM`
- `EEPROM_DIRTY_BLOCK_OFFSET`
- `EEPROM_DIRTY_BLOCK_SYS_FLAG`

### 2.2 旧 flag 暂时保留

为了不改坏原有逻辑，旧的字段级 flag 暂时不删，只是逐步并入块级 dirty。

### 2.3 通信层只负责两件事

通信写入后只做：
- 更新 RAM
- 标记对应 dirty

通信层不再直接负责 EEPROM 细节。

## 3. 已落地的代码状态

### 3.1 已接入 dirty 的块

已经接入块级 dirty 的模块：
- 校准块
- 保护块
- RTC 块
- SOC 表块
- CopperLoss 块
- OtherElement1 块
- HeatCool 块
- 产品信息块
- 事件记录块
- 系统功能位块
- 偏移量块

### 3.2 已恢复的后台写分支

下面这些后台写分支已经恢复，不再依赖注释掉的旧代码：
- `RTC`
- `SOC_TABLE`
- `COPPERLOSS`

### 3.3 新增的关键调试变量

为了便于 Keil 在线调试，新增或保留以下变量：
- `u32EepromDirtyMask`
- `g_u16CurrentCaliOffsetValue`
- `System_OnOFF_Func.all`
- `u8E2P_KB_WriteFlag`
- `u32E2P_Pro_VolCur_WriteFlag`
- `u32E2P_Pro_Temp_WriteFlag`
- `u32E2P_Pro_Other_WriteFlag`
- `u8E2P_SocTable_WriteFlag`
- `u8E2P_CopperLoss_WriteFlag`
- `u32E2P_RTC_Element_WriteFlag`
- `u32E2P_OtherElement1_WriteFlag`
- `u32E2P_HeatCool_WriteFlag`
- `gu8_Reset_EventRecord`
- `System_ErrFlag.u8ErrFlag_Com_EEPROM`
- `MCUO_E2PR_WP`

## 4. Keil 调试顺序

建议按这个顺序看：

1. 通信命令是否先把 RAM 改对
2. 对应 dirty 是否被置位
3. `App_E2promDeal()` 是否被周期调用
4. `WriteEEPROM_ByteData_Circle()` 是否真的进入
5. `WriteEEPROM_Word_NoZone()` 的返回值和回读是否一致
6. `WriteEEPROM_Byte()` 退出时写保护是否恢复
7. `System_ErrFlag.u8ErrFlag_Com_EEPROM` 是否被置位

## 5. 推荐断点

优先在这些函数上打断点：
- `Sci_Deal_WrRegs_0x10()`
- `Sci_WrRegs_0x10_*()`
- `Sci_WrReg_0x06_*()`
- `App_E2promDeal()`
- `WriteEEPROM_ByteData_Circle()`
- `WriteEEPROM_Word_NoZone()`
- `WriteEEPROM_Byte()`
- `ReadEEPROM_ByteData_StartUp()`
- `InitData_E2prom()`

## 6. 当前新增落地

本轮已经把下面两条路径收进统一 dirty：
- `System_OnOFF_Func` -> `EEPROM_DIRTY_BLOCK_SYS_FLAG`
- `DataLoad_CurrentCali_startup()` 的偏移量 -> `EEPROM_DIRTY_BLOCK_OFFSET`

也就是说：
- 系统功能位不再直接在通信层裸写 EEPROM
- 偏移量不再只改 RAM，而是先标记 dirty 再统一落盘

## 7. 结论

现在这套收敛方案的目标不是“彻底重写”，而是：
- 地址保持不变
- 协议保持不变
- 标志位收敛成块级 dirty
- 通信层只改 RAM 和标记 dirty
- EEPROM 层统一落盘

这样最适合 Keil 在线调试，也最容易继续收敛旧逻辑。
