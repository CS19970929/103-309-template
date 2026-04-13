# 通信完整地址索引

本文把 `0x03 / 0x06 / 0x10` 相关地址展开为完整清单。
地址名以源码中的宏为准，便于和 `Sci_Upper.h` 直接对照。

## 1. `0x03` 读寄存器

| 起始地址 | 读区宏 | 读区内容 | 主要代码入口 |
|---|---|---|---|
| 0xD000 | RS485_ADDR_RO_START0 | 主状态区，单体电压/总压/温度/电流/SOC 等 | Sci_ACK_0x03_ReadRegs_Data() |
| 0xD100 | RS485_ADDR_RO_START1 | RTC、故障记录、错误状态、系统状态 | Sci_ACK_0x03_ReadRegs_Data() |
| 0xD200 | RS485_ADDR_RO_START2 | 扩展只读区 | Sci_ACK_0x03_ReadRegs_Data() |
| 0xC000 | RS485_ADDR_RO_LCD | LCD 汇总数据 | Sci_ACK_0x03_ReadRegs_LCD() |
| 0xC001 | RS485_ADDR_RO_FA_RTC | RTC 快速读取 | Sci_ACK_0x03_ReadRegs_LCD() |
| 0xC002 | RS485_ADDR_SN_READ | 序列号 / 硬件版本 / 软件版本 | Sci_ACK_0x03_ReadRegs_LCD() |
| 0xC008 | RS485_ADDR_EVENT_RECORD | 事件记录区 | Sci_ACK_0x03_ReadRegs_LCD() / Sci_ACK_0x03_ReadRegs_EventRecord() |

### 1.1 `0xD000` 字段顺序

- `u16VCell[0..31]` 逐串电压
- `u16VCellMax / u16VCellMin`
- `u16VCellMaxPosition / u16VCellMinPosition`
- `u16VCellDelta / u16VCellTotle`
- `u16Temperature[0..9]`，其中 `TEMP_NUM = 10`
- `u16TempMax / u16TempMin`
- `u16Ichg / u16IDischg`
- `SocElement`、三组故障位、均衡标志位

#### 1.1.1 `0xD000` 寄存器级展开

`0xD000` 区按结构体顺序连续输出，共 63 个 halfword，当前对应关系如下：

| 偏移 | 数据 |
|---|---|
| 0 ~ 31 | `u16VCell[0..31]` |
| 32 | `u16VCellMax` |
| 33 | `u16VCellMin` |
| 34 | `u16VCellMaxPosition` |
| 35 | `u16VCellMinPosition` |
| 36 | `u16VCellDelta` |
| 37 | `u16VCellTotle` |
| 38 ~ 47 | `u16Temperature[0..9]` |
| 48 | `u16TempMax` |
| 49 | `u16TempMin` |
| 50 | `u16Ichg` |
| 51 | `u16IDischg` |
| 52 | `SocElement.u16Soc` |
| 53 | `SocElement.u16Soh` |
| 54 | `SocElement.u16CapacityNow` |
| 55 | `SocElement.u16CapacityFull` |
| 56 | `SocElement.u16CapacityFactory` |
| 57 | `SocElement.u16Cycle_times` |
| 58 | `unMdlFault_First.all` |
| 59 | `unMdlFault_Second.all` |
| 60 | `unMdlFault_Third.all` |
| 61 | `u16BalanceFlag1` |
| 62 | `u16BalanceFlag2` |

### 1.2 `0xD100` 字段顺序

- RTC 时间
- 三段故障记录及对应 RTC 记录
- 系统错误位 / 开关机状态

#### 1.2.1 `0xD100` 寄存器级展开

`0xD100` 区当前实现按下面顺序打包。代码里有预留零值寄存器，所以文档以“字段顺序 + 预留位”方式记录，避免和抓包长度混淆。

| 顺序 | 数据 |
|---|---|
| 0 | `RTC_time.RTC_Time_Year / RTC_time.RTC_Time_Month` |
| 1 | `RTC_time.RTC_Time_Day / RTC_time.RTC_Time_Hour` |
| 2 | `RTC_time.RTC_Time_Minute / RTC_time.RTC_Time_Second` |
| 3 ~ 4 | `Fault_record_First2` 打包字段 |
| 5 ~ 6 | `Fault_record_Second2` 打包字段 |
| 7 ~ 8 | `Fault_record_Third2` 打包字段 |
| 9 ~ 20 | `System_ErrFlag` 12 个寄存器，按字节对打包 |
| 21 | `SystemStatus` 低 16 位 |
| 22 | `SystemStatus` 高 16 位 |
| 23 | `System_OnOFF_Func` 低 16 位 |
| 24 | `System_OnOFF_Func` 高 16 位 |
| 25 ~ 26 | 预留零值寄存器 |
| 27 | `Heat_Cool_FaultFlag.all` |
| 28 ~ 33 | 预留零值寄存器 |

### 1.2.2 读区说明

`0xD100` 区的用途不是只做一个单一数据块，而是把 RTC、三段历史故障、系统状态和开关机状态拼在一起，方便上位机一次性读完。

## 2. `0x06` 单寄存器控制 / 复位

| 地址 | 宏名 | 含义 | 触发函数 |
|---|---|---|---|
| 0x1000 | `RS485_CMD_ADDR_RESET_CALIB_COEF` | 以源码命名为准 | `Sci_WrReg_0x06_*()` |
| 0x1001 | `RS485_CMD_ADDR_RESET_PROTECT_RECORD` | 以源码命名为准 | `Sci_WrReg_0x06_*()` |
| 0x1002 | `RS485_CMD_ADDR_RESET_PROTECT_ELEMENT` | 以源码命名为准 | `Sci_WrReg_0x06_*()` |
| 0x1003 | `RS485_CMD_ADDR_RESET_OTHER_CANADD` | 以源码命名为准 | `Sci_WrReg_0x06_*()` |
| 0x1004 | `RS485_CMD_ADDR_RESET_HEAT_COOL` | 以源码命名为准 | `Sci_WrReg_0x06_*()` |
| 0x1005 | `RS485_CMD_ADDR_SET_ONCE_SOC` | 以源码命名为准 | `Sci_WrReg_0x06_*()` |
| 0x1006 | `RS485_CMD_ADDR_RESET_AFE_PARAMETERS` | 以源码命名为准 | `Sci_WrReg_0x06_*()` |
| 0x1007 | `RS485_CMD_ADDR_RESET_EVENT_RECORD` | 以源码命名为准 | `Sci_WrReg_0x06_*()` |
| 0x1100 | `RS485_CMD_ADDR_SWITCH_ON` | 以源码命名为准 | `Sci_WrReg_0x06_*()` |
| 0x1101 | `RS485_CMD_ADDR_SWITCH_OFF` | 以源码命名为准 | `Sci_WrReg_0x06_*()` |
| 0x1102 | `RS485_CMD_ADDR_SYSTEM_FUNCTION_ON` | 以源码命名为准 | `Sci_WrReg_0x06_*()` |
| 0x1103 | `RS485_CMD_ADDR_SYSTEM_FUNCTION_OFF` | 以源码命名为准 | `Sci_WrReg_0x06_*()` |

## 3. `0x10` 多寄存器写入

| 起始地址 | 宏名 | 含义 | 主要代码入口 |
|---|---|---|---|
| 0x2000 | `RS485_CMD_ADDR_VC1CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2001 | `RS485_CMD_ADDR_VC1CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2002 | `RS485_CMD_ADDR_VC2CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2003 | `RS485_CMD_ADDR_VC2CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2004 | `RS485_CMD_ADDR_VC3CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2005 | `RS485_CMD_ADDR_VC3CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2006 | `RS485_CMD_ADDR_VC4CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2007 | `RS485_CMD_ADDR_VC4CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2008 | `RS485_CMD_ADDR_VC5CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2009 | `RS485_CMD_ADDR_VC5CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x200A | `RS485_CMD_ADDR_VC6CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x200B | `RS485_CMD_ADDR_VC6CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x200C | `RS485_CMD_ADDR_VC7CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x200D | `RS485_CMD_ADDR_VC7CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x200E | `RS485_CMD_ADDR_VC8CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x200F | `RS485_CMD_ADDR_VC8CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2010 | `RS485_CMD_ADDR_VC9CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2011 | `RS485_CMD_ADDR_VC9CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2012 | `RS485_CMD_ADDR_VC10CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2013 | `RS485_CMD_ADDR_VC10CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2014 | `RS485_CMD_ADDR_VC11CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2015 | `RS485_CMD_ADDR_VC11CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2016 | `RS485_CMD_ADDR_VC12CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2017 | `RS485_CMD_ADDR_VC12CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2018 | `RS485_CMD_ADDR_VC13CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2019 | `RS485_CMD_ADDR_VC13CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x201A | `RS485_CMD_ADDR_VC14CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x201B | `RS485_CMD_ADDR_VC14CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x201C | `RS485_CMD_ADDR_VC15CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x201D | `RS485_CMD_ADDR_VC15CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x201E | `RS485_CMD_ADDR_VC16CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x201F | `RS485_CMD_ADDR_VC16CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2020 | `RS485_CMD_ADDR_VC17CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2021 | `RS485_CMD_ADDR_VC17CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2022 | `RS485_CMD_ADDR_VC18CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2023 | `RS485_CMD_ADDR_VC18CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2024 | `RS485_CMD_ADDR_VC19CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2025 | `RS485_CMD_ADDR_VC19CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2026 | `RS485_CMD_ADDR_VC20CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2027 | `RS485_CMD_ADDR_VC20CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2028 | `RS485_CMD_ADDR_VC21CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2029 | `RS485_CMD_ADDR_VC21CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x202A | `RS485_CMD_ADDR_VC22CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x202B | `RS485_CMD_ADDR_VC22CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x202C | `RS485_CMD_ADDR_VC23CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x202D | `RS485_CMD_ADDR_VC23CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x202E | `RS485_CMD_ADDR_VC24CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x202F | `RS485_CMD_ADDR_VC24CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2030 | `RS485_CMD_ADDR_VC25CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2031 | `RS485_CMD_ADDR_VC25CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2032 | `RS485_CMD_ADDR_VC26CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2033 | `RS485_CMD_ADDR_VC26CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2034 | `RS485_CMD_ADDR_VC27CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2035 | `RS485_CMD_ADDR_VC27CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2036 | `RS485_CMD_ADDR_VC28CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2037 | `RS485_CMD_ADDR_VC28CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2038 | `RS485_CMD_ADDR_VC29CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2039 | `RS485_CMD_ADDR_VC29CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x203A | `RS485_CMD_ADDR_VC30CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x203B | `RS485_CMD_ADDR_VC30CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x203C | `RS485_CMD_ADDR_VC31CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x203D | `RS485_CMD_ADDR_VC31CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x203E | `RS485_CMD_ADDR_VC32CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x203F | `RS485_CMD_ADDR_VC32CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2040 | `RS485_CMD_ADDR_AFE1CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2041 | `RS485_CMD_ADDR_AFE1CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2042 | `RS485_CMD_ADDR_AFE2CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2043 | `RS485_CMD_ADDR_AFE2CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2044 | `RS485_CMD_ADDR_VBUSCALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2045 | `RS485_CMD_ADDR_VBUSCALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2046 | `RS485_CMD_ADDR_ICHGCALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2047 | `RS485_CMD_ADDR_ICHGCALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2048 | `RS485_CMD_ADDR_IDISCHGCALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2049 | `RS485_CMD_ADDR_IDISCHGCALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x204A | `RS485_CMD_ADDR_TEMP1_CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x204B | `RS485_CMD_ADDR_TEMP1_CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x204C | `RS485_CMD_ADDR_TEMP2_CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x204D | `RS485_CMD_ADDR_TEMP2_CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x204E | `RS485_CMD_ADDR_TEMP3_CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x204F | `RS485_CMD_ADDR_TEMP3_CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2050 | `RS485_CMD_ADDR_TEMP4_CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2051 | `RS485_CMD_ADDR_TEMP4_CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2052 | `RS485_CMD_ADDR_TEMP5_CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2053 | `RS485_CMD_ADDR_TEMP5_CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2054 | `RS485_CMD_ADDR_TEMP6_CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2055 | `RS485_CMD_ADDR_TEMP6_CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2056 | `RS485_CMD_ADDR_TEMP_ENV1_CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2057 | `RS485_CMD_ADDR_TEMP_ENV1_CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2058 | `RS485_CMD_ADDR_TEMP_ENV2_CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2059 | `RS485_CMD_ADDR_TEMP_ENV2_CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x205A | `RS485_CMD_ADDR_TEMP_ENV3_CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x205B | `RS485_CMD_ADDR_TEMP_ENV3_CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x205C | `RS485_CMD_ADDR_TEMP_MOS_CALIB_K` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x205D | `RS485_CMD_ADDR_TEMP_MOS_CALIB_B` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2100 | `RS485_CMD_ADDR_VCELL_OVP_FIRST` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2101 | `RS485_CMD_ADDR_VCELL_OVP_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2102 | `RS485_CMD_ADDR_VCELL_OVP_THIRD` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2103 | `RS485_CMD_ADDR_VCELL_OVP_RCV` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2104 | `RS485_CMD_ADDR_VCELL_OVP_FILTER` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2105 | `RS485_CMD_ADDR_VCELL_UVP_FIRST` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2106 | `RS485_CMD_ADDR_VCELL_UVP_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2107 | `RS485_CMD_ADDR_VCELL_UVP_THIRD` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2108 | `RS485_CMD_ADDR_VCELL_UVP_RCV` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2109 | `RS485_CMD_ADDR_VCELL_UVP_FILTER` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x210A | `RS485_CMD_ADDR_VBUS_OVP_FIRST` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x210B | `RS485_CMD_ADDR_VBUS_OVP_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x210C | `RS485_CMD_ADDR_VBUS_OVP_THIRD` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x210D | `RS485_CMD_ADDR_VBUS_OVP_RCV` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x210E | `RS485_CMD_ADDR_VBUS_OVP_FILTER` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x210F | `RS485_CMD_ADDR_VBUS_UVP_FIRST` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2110 | `RS485_CMD_ADDR_VBUS_UVP_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2111 | `RS485_CMD_ADDR_VBUS_UVP_THIRD` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2112 | `RS485_CMD_ADDR_VBUS_UVP_RCV` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2113 | `RS485_CMD_ADDR_VBUS_UVP_FILTER` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2114 | `RS485_CMD_ADDR_ICHG_OCP_FIRST` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2115 | `RS485_CMD_ADDR_ICHG_OCP_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2116 | `RS485_CMD_ADDR_ICHG_OCP_THIRD` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2117 | `RS485_CMD_ADDR_ICHG_OCP_RCV` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2118 | `RS485_CMD_ADDR_ICHG_OCP_FILTER` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2119 | `RS485_CMD_ADDR_IDSG_OCP_FIRST` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x211A | `RS485_CMD_ADDR_IDSG_OCP_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x211B | `RS485_CMD_ADDR_IDSG_OCP_THIRD` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x211C | `RS485_CMD_ADDR_IDSG_OCP_RCV` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x211D | `RS485_CMD_ADDR_IDSG_OCP_FILTER` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x211E | `RS485_CMD_ADDR_TCHG_OTP_FIRST` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x211F | `RS485_CMD_ADDR_TCHG_OTP_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2120 | `RS485_CMD_ADDR_TCHG_OTP_THIRD` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2121 | `RS485_CMD_ADDR_TCHG_OTP_RCV` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2122 | `RS485_CMD_ADDR_TCHG_OTP_FILTER` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2123 | `RS485_CMD_ADDR_TCHG_UTP_FIRST` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2124 | `RS485_CMD_ADDR_TCHG_UTP_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2125 | `RS485_CMD_ADDR_TCHG_UTP_THIRD` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2126 | `RS485_CMD_ADDR_TCHG_UTP_RCV` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2127 | `RS485_CMD_ADDR_TCHG_UTP_FILTER` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2128 | `RS485_CMD_ADDR_TDSG_OTP_FIRST` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2129 | `RS485_CMD_ADDR_TDSG_OTP_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x212A | `RS485_CMD_ADDR_TDSG_OTP_THIRD` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x212B | `RS485_CMD_ADDR_TDSG_OTP_RCV` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x212C | `RS485_CMD_ADDR_TDSG_OTP_FILTER` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x212D | `RS485_CMD_ADDR_TDSG_UTP_FIRST` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x212E | `RS485_CMD_ADDR_TDSG_UTP_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x212F | `RS485_CMD_ADDR_TDSG_UTP_THIRD` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2130 | `RS485_CMD_ADDR_TDSG_UTP_RCV` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2131 | `RS485_CMD_ADDR_TDSG_UTP_FILTER` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2132 | `RS485_CMD_ADDR_TMOS_OTP_FIRST` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2133 | `RS485_CMD_ADDR_TMOS_OTP_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2134 | `RS485_CMD_ADDR_TMOS_OTP_THIRD` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2135 | `RS485_CMD_ADDR_TMOS_OTP_RCV` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2136 | `RS485_CMD_ADDR_TMOS_OTP_FILTER` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2137 | `RS485_CMD_ADDR_VDELTA_OP_FIRST` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2138 | `RS485_CMD_ADDR_VDELTA_OP_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2139 | `RS485_CMD_ADDR_VDELTA_OP_THIRD` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x213A | `RS485_CMD_ADDR_VDELTA_OP_RCV` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x213B | `RS485_CMD_ADDR_VDELTA_OP_FILTER` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x213C | `RS485_CMD_ADDR_SOC_UP_FIRST` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x213D | `RS485_CMD_ADDR_SOC_UP_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x213E | `RS485_CMD_ADDR_SOC_UP_THIRD` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x213F | `RS485_CMD_ADDR_SOC_UP_RCV` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2140 | `RS485_CMD_ADDR_SOC_UP_FILTER` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2200 | `RS485_CMD_ADDR_SOC_VOLTAGE1` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2201 | `RS485_CMD_ADDR_SOC_VALUE1` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2202 | `RS485_CMD_ADDR_SOC_VOLTAGE2` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2203 | `RS485_CMD_ADDR_SOC_VALUE2` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2204 | `RS485_CMD_ADDR_SOC_VOLTAGE3` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2205 | `RS485_CMD_ADDR_SOC_VALUE3` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2206 | `RS485_CMD_ADDR_SOC_VOLTAGE4` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2207 | `RS485_CMD_ADDR_SOC_VALUE4` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2208 | `RS485_CMD_ADDR_SOC_VOLTAGE5` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2209 | `RS485_CMD_ADDR_SOC_VALUE5` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x220A | `RS485_CMD_ADDR_SOC_VOLTAGE6` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x220B | `RS485_CMD_ADDR_SOC_VALUE6` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x220C | `RS485_CMD_ADDR_SOC_VOLTAGE7` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x220D | `RS485_CMD_ADDR_SOC_VALUE7` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x220E | `RS485_CMD_ADDR_SOC_VOLTAGE8` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x220F | `RS485_CMD_ADDR_SOC_VALUE8` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2210 | `RS485_CMD_ADDR_SOC_VOLTAGE9` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2211 | `RS485_CMD_ADDR_SOC_VALUE9` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2212 | `RS485_CMD_ADDR_SOC_VOLTAGE10` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2213 | `RS485_CMD_ADDR_SOC_VALUE10` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2214 | `RS485_CMD_ADDR_SOC_VOLTAGE11` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2215 | `RS485_CMD_ADDR_SOC_VALUE11` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2216 | `RS485_CMD_ADDR_SOC_VOLTAGE12` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2217 | `RS485_CMD_ADDR_SOC_VALUE12` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2218 | `RS485_CMD_ADDR_SOC_VOLTAGE13` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2219 | `RS485_CMD_ADDR_SOC_VALUE13` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x221A | `RS485_CMD_ADDR_SOC_VOLTAGE14` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x221B | `RS485_CMD_ADDR_SOC_VALUE14` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x221C | `RS485_CMD_ADDR_SOC_VOLTAGE15` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x221D | `RS485_CMD_ADDR_SOC_VALUE15` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x221E | `RS485_CMD_ADDR_SOC_VOLTAGE16` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x221F | `RS485_CMD_ADDR_SOC_VALUE16` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2220 | `RS485_CMD_ADDR_SOC_VOLTAGE17` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2221 | `RS485_CMD_ADDR_SOC_VALUE17` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2222 | `RS485_CMD_ADDR_SOC_VOLTAGE18` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2223 | `RS485_CMD_ADDR_SOC_VALUE18` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2224 | `RS485_CMD_ADDR_SOC_VOLTAGE19` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2225 | `RS485_CMD_ADDR_SOC_VALUE19` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2226 | `RS485_CMD_ADDR_SOC_VOLTAGE20` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2227 | `RS485_CMD_ADDR_SOC_VALUE20` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2228 | `RS485_CMD_ADDR_SOC_VOLTAGE21` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2229 | `RS485_CMD_ADDR_SOC_VALUE21` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x222A | `RS485_CMD_ADDR_COPPERLOSS1` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x222B | `RS485_CMD_ADDR_COPPERLOSS2` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x222C | `RS485_CMD_ADDR_COPPERLOSS3` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x222D | `RS485_CMD_ADDR_COPPERLOSS4` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x222E | `RS485_CMD_ADDR_COPPERLOSS5` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x222F | `RS485_CMD_ADDR_COPPERLOSS6` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2230 | `RS485_CMD_ADDR_COPPERLOSS7` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2231 | `RS485_CMD_ADDR_COPPERLOSS8` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2232 | `RS485_CMD_ADDR_COPPERLOSS9` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2233 | `RS485_CMD_ADDR_COPPERLOSS10` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2234 | `RS485_CMD_ADDR_COPPERLOSS11` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2235 | `RS485_CMD_ADDR_COPPERLOSS12` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2236 | `RS485_CMD_ADDR_COPPERLOSS13` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2237 | `RS485_CMD_ADDR_COPPERLOSS14` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2238 | `RS485_CMD_ADDR_COPPERLOSS15` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2239 | `RS485_CMD_ADDR_COPPERLOSS16` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x223A | `RS485_CMD_ADDR_CELLNUM1` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x223B | `RS485_CMD_ADDR_CELLNUM2` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x223C | `RS485_CMD_ADDR_CELLNUM3` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x223D | `RS485_CMD_ADDR_CELLNUM4` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x223E | `RS485_CMD_ADDR_CELLNUM5` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x223F | `RS485_CMD_ADDR_CELLNUM6` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2240 | `RS485_CMD_ADDR_CELLNUM7` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2241 | `RS485_CMD_ADDR_CELLNUM8` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2242 | `RS485_CMD_ADDR_CELLNUM9` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2243 | `RS485_CMD_ADDR_CELLNUM10` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2244 | `RS485_CMD_ADDR_CELLNUM11` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2245 | `RS485_CMD_ADDR_CELLNUM12` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2246 | `RS485_CMD_ADDR_CELLNUM13` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2247 | `RS485_CMD_ADDR_CELLNUM14` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2248 | `RS485_CMD_ADDR_CELLNUM15` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2249 | `RS485_CMD_ADDR_CELLNUM16` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x224A | `RS485_CMD_ADDR_RTC_TIME_YEAR` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x224B | `RS485_CMD_ADDR_RTC_TIME_MONTH` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x224C | `RS485_CMD_ADDR_RTC_TIME_DAY` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x224D | `RS485_CMD_ADDR_RTC_TIME_HOUR` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x224E | `RS485_CMD_ADDR_RTC_TIME_MINUTE` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x224F | `RS485_CMD_ADDR_RTC_TIME_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2250 | `RS485_CMD_ADDR_RTC_ALARM_YEAR` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2251 | `RS485_CMD_ADDR_RTC_ALARM_MONTH` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2252 | `RS485_CMD_ADDR_RTC_ALARM_DAY` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2253 | `RS485_CMD_ADDR_RTC_ALARM_HOUR` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2254 | `RS485_CMD_ADDR_RTC_ALARM_MINUTE` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2255 | `RS485_CMD_ADDR_RTC_ALARM_SECOND` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2300 | `RS485_CMD_ADDR_BALANCE_OV` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2301 | `RS485_CMD_ADDR_BALANCE_OW` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2302 | `RS485_CMD_ADDR_BALANCE_CW1` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2303 | `RS485_CMD_ADDR_BALANCE_CW2` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2304 | `RS485_CMD_ADDR_OPENTIME_ODD` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2305 | `RS485_CMD_ADDR_OPENTIME_EVEN` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2306 | `RS485_CMD_ADDR_OPENTIME_MOS` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2307 | `RS485_CMD_ADDR_OPENTIME_RES` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2308 | `RS485_CMD_ADDR_CS_CUR_CHGMAX` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2309 | `RS485_CMD_ADDR_CS_CUR_DSGMAX` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x230A | `RS485_CMD_ADDR_CBC_CUR_CHG` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x230B | `RS485_CMD_ADDR_CBC_CUR_DSG` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x230C | `RS485_CMD_ADDR_COOL_DSG_H` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x230D | `RS485_CMD_ADDR_COOL_DSG_L` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x230E | `RS485_CMD_ADDR_COOL_CHG_H` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x230F | `RS485_CMD_ADDR_COOL_CHG_L` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2310 | `RS485_CMD_ADDR_SLEEP_V_NORMAL` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2311 | `RS485_CMD_ADDR_SLEEP_TIME_NORMAL` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2312 | `RS485_CMD_ADDR_SLEEP_V_LOW` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2313 | `RS485_CMD_ADDR_SLEEP_TIME_LOW` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2314 | `RS485_CMD_ADDR_SLEEP_I_CHG` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2315 | `RS485_CMD_ADDR_SLEEP_I_DSG` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2316 | `RS485_CMD_ADDR_SLEEP_RES1` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2317 | `RS485_CMD_ADDR_SLEEP_RES2` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2318 | `RS485_CMD_ADDR_SOC_AH` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2319 | `RS485_CMD_ADDR_SOC_CYCLE_TIME` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x231A | `RS485_CMD_ADDR_SOC_RES1` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x231B | `RS485_CMD_ADDR_SOC_RES2` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x231C | `RS485_CMD_ADDR_SYS_SERIES_NUM` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x231D | `RS485_CMD_ADDR_SYS_CS_RESIS` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x231E | `RS485_CMD_ADDR_SYS_CS_NUM` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x231F | `RS485_CMD_ADDR_SYS_RES1` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2320 | `RS485_CMD_ADDR_HEAT_DSG_HIGH` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2321 | `RS485_CMD_ADDR_HEAT_DSG_LOW` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2322 | `RS485_CMD_ADDR_HEAT_CHG_HIGH` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2323 | `RS485_CMD_ADDR_HEAT_CHG_LOW` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2324 | `RS485_CMD_ADDR_HEAT_CUR_MAX` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2325 | `RS485_CMD_ADDR_HEAT_CUR_MIN` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2326 | `RS485_CMD_ADDR_HEAT_TIME_MAX` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2327 | `RS485_CMD_ADDR_HEAT_RES1` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2328 | `RS485_CMD_ADDR_HEAT_RES2` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2329 | `RS485_CMD_ADDR_HEAT_RES3` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x232A | `RS485_CMD_ADDR_HEAT_RES4` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x232B | `RS485_CMD_ADDR_HEAT_RES5` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x232C | `RS485_CMD_ADDR_HEAT_RES6` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x232D | `RS485_CMD_ADDR_COOL_DSG_HIGH` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x232E | `RS485_CMD_ADDR_COOL_DSG_LOW` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x232F | `RS485_CMD_ADDR_COOL_CHG_HIGH` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2330 | `RS485_CMD_ADDR_COOL_CHG_LOW` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2331 | `RS485_CMD_ADDR_COOL_CUR_MAX` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2332 | `RS485_CMD_ADDR_COOL_CUR_MIN` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2333 | `RS485_CMD_ADDR_COOL_TIME_MAX` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2334 | `RS485_CMD_ADDR_COOL_RES1` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2335 | `RS485_CMD_ADDR_COOL_RES2` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2336 | `RS485_CMD_ADDR_COOL_RES3` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |
| 0x2337 | `RS485_CMD_ADDR_COOL_RES4` | 以源码命名为准 | `Sci_WrRegs_0x10_*()` |

### 3.1 `0x2400 ~ 0x2417` AFE 参数块

- 起始地址：`0x2400`
- 结束地址：`0x2417`
- 总长度：`24` 个寄存器
- 代码入口：`Sci_WrRegs_0x10_AFE_Parameters()`

### 3.2 额外写入地址

| 地址 | 宏名 | 含义 | 主要代码入口 |
|---|---|---|---|
| 0xFFF0 | `RS485_ADDR_SN_SERIAL_NUM` | 序列号 | `Sci_WrRegs_0x10_SN_Version()` |
| 0xFFF1 | `RS485_ADDR_SN_HAEDWARE_VER` | 硬件版本 | `Sci_WrRegs_0x10_SN_Version()` |
| 0xFFF2 | `RS485_ADDR_SN_SOFTWARE_VER` | 软件版本 | `Sci_WrRegs_0x10_SN_Version()` |
| 0xFFFD | `RS485_CMD_ADDR_FLASH_CONNECT` | Flash 更新触发 | `Sci_WrRegs_0x10_FlashConnect()` |

## 4. `0xC000 ~ 0xC008` 读区索引

### 4.1 `0xC000` LCD 汇总区

| 顺序 | 数据 |
|---|---|
| 0 | 固定版本值 `1` |
| 1 | 总电压 |
| 2 | 电流 |
| 3 | 最高温度 |
| 4 | SOC |

### 4.2 `0xC001` RTC 区

`0xC001` 作为 RTC 快速读取区，当前主要用于上位机快速拿时间类数据，逻辑上与 `0xD100` 的 RTC 读取一致。

### 4.3 `0xC002` SN / 版本区

| 顺序 | 数据 |
|---|---|
| 0 ~ `PRODUCT_ID_LENGTH_MAX-1` | 序列号 |
| `PRODUCT_ID_LENGTH_MAX` ~ `2*PRODUCT_ID_LENGTH_MAX-1` | 硬件版本 |
| `2*PRODUCT_ID_LENGTH_MAX` ~ `3*PRODUCT_ID_LENGTH_MAX-1` | 软件版本 |

### 4.4 `0xC008` 事件记录区

| 顺序 | 数据 |
|---|---|
| 0 ~ 199 | 100 条事件记录，每条 2 字节 |

## 5. 读取与写入的索引规则

- `0x03`：起始地址决定读区，数据按寄存器连续返回。
- `0x06`：地址本身就是命令值，单寄存器写后执行控制动作。
- `0x10`：起始地址决定参数块，寄存器数量决定写入长度。
- 绝大多数 `0x10` 写入会先改 RAM，再置写标志，最后由后台统一落盘。

## 6. 备注

- `0x2000 ~ 0x205D` 为校准系数区。
- `0x2100 ~ 0x2140` 为保护参数区。
- `0x2200 ~ 0x2255` 为 SOC / 铜损 / RTC 区。
- `0x2300 ~ 0x2337` 为均衡、系统、睡眠、SOC 扩展、热管理区。
- `0xFFF0 ~ 0xFFF2` 为产品 ID。
- `0xFFFD` 为 Flash 更新触发。

