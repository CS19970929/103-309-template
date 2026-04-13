# 通信 EEPROM 标志位映射表

本文是 `COMMUNICATION_EEPROM_FLAG_REFACTOR_DEBUG.md` 的配套表，用于把旧的字段级写标志映射到新的块级 dirty。

目标不变：
- EEPROM 地址不变
- 通信地址不变
- 逻辑更简单
- Keil 更容易观察

## 1. 新的块级 dirty

建议最终保留的块级 dirty 如下：

| 位号 | 块名 | 说明 |
|---|---|---|
| 0 | `CALIB` | K/B 校准 |
| 1 | `PROTECT` | 保护参数 |
| 2 | `RTC` | RTC 数据 |
| 3 | `SOC_TABLE` | SOC 表 |
| 4 | `COPPERLOSS` | 铜损 |
| 5 | `OTHER1` | OtherElement1 |
| 6 | `HEAT_COOL` | 热管理参数 |
| 7 | `PRODUCT_INFO` | 序列号/版本号 |
| 8 | `EVENT_RECORD` | 事件记录 |
| 9 | `AFE_PARAM` | AFE 参数 |
| 10 | `OFFSET` | 虚拟电流偏移量 |
| 11 | `SYS_FLAG` | 系统功能开关 |

## 2. 旧 flag 到新 dirty 的映射

| 旧 flag / 旧路径 | 新 dirty | 说明 |
|---|---|---|
| `u8E2P_KB_WriteFlag` | `CALIB` | 校准 K/B 表 |
| `u32E2P_Pro_VolCur_WriteFlag` | `PROTECT` | 保护电压/电流 |
| `u32E2P_Pro_Temp_WriteFlag` | `PROTECT` | 保护温度 |
| `u32E2P_Pro_Other_WriteFlag` | `PROTECT` | 其他保护参数 |
| `u32E2P_RTC_Element_WriteFlag` | `RTC` | RTC 时间和相关字段 |
| `u8E2P_SocTable_WriteFlag` | `SOC_TABLE` | SOC 表 |
| `u8E2P_CopperLoss_WriteFlag` | `COPPERLOSS` | 铜损和数量表 |
| `u32E2P_OtherElement1_WriteFlag` | `OTHER1` | OtherElement1 区 |
| `u32E2P_HeatCool_WriteFlag` | `HEAT_COOL` | 热管理参数 |
| `gu8_Reset_EventRecord` | `EVENT_RECORD` | 事件记录清空 |
| `ProductionInfor.BMS_*_WriteFlag` | `PRODUCT_INFO` | 序列号和版本号 |
| `System_OnOFF_Func` 保存 | `SYS_FLAG` | 系统功能选择位 |
| `FLASH_ADDR_SH367309_VALUE` 的写入 | `OFFSET` | 虚拟电流偏移量 |

## 3. 通信地址和 EEPROM 的关系

通信地址不改，EEPROM 物理地址也不改。这里只是把写入流程从“字段级直接写”收敛成“块级 dirty 调度”。

也就是说：
- `0x03` 还是读寄存器
- `0x06` 还是单寄存器控制
- `0x10` 还是多寄存器写入
- 只是写入后不再到处散落直接写 EEPROM

## 4. Keil 调试时怎么看

建议优先观察：

1. `u32EepromDirtyMask`
2. `System_OnOFF_Func.all`
3. `g_u16CurrentCaliOffsetValue`
4. `u8E2P_KB_WriteFlag`
5. `u32E2P_Pro_VolCur_WriteFlag`
6. `u32E2P_Pro_Temp_WriteFlag`
7. `u32E2P_Pro_Other_WriteFlag`
8. `u8E2P_SocTable_WriteFlag`
9. `u8E2P_CopperLoss_WriteFlag`
10. `u32E2P_RTC_Element_WriteFlag`
11. `u32E2P_OtherElement1_WriteFlag`
12. `u32E2P_HeatCool_WriteFlag`

## 5. 推荐断点

如果要追写入链路，建议断在：
- `Sci_Deal_WrRegs_0x10()`
- `Sci_WrReg_0x06_BMS_FunctionON()`
- `Sci_WrReg_0x06_BMS_FunctionOFF()`
- `SystemMonitorResetData_EEPROM()`
- `DataLoad_CurrentCali_startup()`
- `App_E2promDeal()`
- `WriteEEPROM_ByteData_Circle()`

## 6. 当前落地状态

已经接入的新路径：
- `System_OnOFF_Func` -> `SYS_FLAG`
- `DataLoad_CurrentCali_startup()` -> `OFFSET`

因此这两条路径现在可以直接在 Keil 里看：
- dirty 是否置位
- 后台是否写入
- 写完是否清位

## 7. 后续建议

下一步最适合继续做的是：
- 继续清理旧字段级 flag 的散落使用
- 逐步把所有写入口都收进块级 dirty
- 保留通信地址和 EEPROM 地址不变

这样能把现有逻辑压缩成更少的调试点，更适合现场定位问题。
