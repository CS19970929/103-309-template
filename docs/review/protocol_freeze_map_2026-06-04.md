# 协议冻结映射表

状态：部分验证。本文基于当前源码整理 SCI/Modbus 与 CAN APP 的外部行为冻结点，不修改源码。

## 目录

- [范围](#范围)
- [SCI/Modbus 帧入口](#scimodbus-帧入口)
- [SCI 读寄存器窗口](#sci-读寄存器窗口)
- [`0xD000` 实时只读窗口](#0xd000-实时只读窗口)
- [SCI 写寄存器窗口](#sci-写寄存器窗口)
- [CAN APP 协议](#can-app-协议)
- [CAN 与 SCI 的耦合](#can-与-sci-的耦合)
- [重构前禁止改变项](#重构前禁止改变项)
- [需人工确认项](#需人工确认项)

## 范围

| 项 | 源码依据 |
| --- | --- |
| Modbus 命令定义 | `103 + 309/Project/Source/Sci_Upper.h` |
| Modbus 解析/读写/ACK | `103 + 309/Project/Source/Sci_Upper.c` |
| CAN APP 命令定义/处理 | `103 + 309/Project/Source/Can_HDX.c` |
| CAN 周期帧字段 | `103 + 309/Project/Source/CanFeidaoFrames.c`，本文仅列入口，不逐帧展开 |

## SCI/Modbus 帧入口

| 命令 | 值 | 处理函数 | 当前行为 |
| --- | --- | --- | --- |
| `RS485_CMD_READ_REGS` | `0x03` | `Sci_Deal_ReadRegs_0x03()` + `Sci_ACK_0x03()` | 校验读窗口与长度，按窗口拼 ACK 数据 |
| `RS485_CMD_WRITE_REG` | `0x06` | `Sci_Deal_WrReg_0x06()` + `Sci_ACK_0x06_0x10()` | 仅处理少量命令寄存器；不用于普通参数区单 word 写 |
| `RS485_CMD_WRITE_REGS` | `0x10` | `Sci_Deal_WrRegs_0x10()` + `Sci_ACK_0x06_0x10()` | 写 AFE/保护/OtherElement/校准/SN/IAP 等参数 |

关键运行路径：

```text
USART1_IRQHandler
└── Sci1_CommonUpper_IRQHandler
    └── Sci_PortIRQHandler
        ├── RXNE: SleepDeal_RecordExternalComm + Sci_ModbusProtocolFeed
        ├── IDLE: incomplete frame reset
        ├── TXE: byte transmit
        └── TC: Sci_PortFinishTx

Runtime_RunOnce
└── AppInit_ServiceSci
    └── App_CommonUpper
        └── Sci_PortService
            └── Sci_ModbusProcessFrame
                ├── CRC_verify
                ├── Sci_Deal_ReadRegs_0x03 / Sci_Deal_WrReg_0x06 / Sci_Deal_WrRegs_0x10
                └── Sci_ACK_0x03 / Sci_ACK_0x06_0x10
```

## SCI 读寄存器窗口

`Sci_Deal_ReadRegs_0x03()` 会把外部地址转换为内部 offset，然后用 `Sci_GetReadWindowWordCount()` 校验窗口长度。

| 地址范围/起点 | 宏 | 当前 word 数 | ACK 数据来源 | 说明 |
| --- | --- | --- | --- | --- |
| `0x2000` 起 | `RS485_ADDR_RW_CALIB` | `KB_NUM * 2` = 94 | `g_u16CalibCoefK[]` + `g_i16CalibCoefB[]` | K/B 交替输出 |
| `0x2100` 起 | `RS485_ADDR_RW_PORTECT` | `E2P_PARA_NUM_PROTECT` = 65 | `PRT_E2ROMParas` | 保护参数 |
| `0x2200` 起 | `RS485_ADDR_RW_OTHER` | `SOC_TABLE_SIZE + 16 + 16 + E2P_PARA_NUM_RTC` = 86 | SOC 表 + 32 word 兼容 padding + RTC | 当前写 SOC 表负应答，但读仍输出 |
| `0x2300` 起 | `RS485_ADDR_RW_OTHER_CANADD` | `E2P_PARA_NUM_OTHER_ELEMENT1` = 32 | `OtherElement` | 其他系统/SOC/睡眠参数 |
| AFE 参数区 | `RS485_ADDR_RW_AFE_PARAMETER` | `AFE_PARAMETES_TOTAL_LENGTH` = 24 | AFE 参数 | 具体起点来自 AFE 头文件 |
| `0xC000` | `RS485_ADDR_RO_LCD` | 5 | 当前 LCD 分支未完整展开 | 需人工确认实际上位机是否读取 |
| `0xC001` | `RS485_ADDR_RO_FA_RTC` | `Record_len * 7` = 70 | `Fault_record_Third` 等 | 上位机三级保护历史记录 |
| `0xC002` | `RS485_ADDR_SN_READ` | `(PRODUCT_ID_LENGTH_MAX * 3 + 1) / 2` = 48 | SN/HW/SW 三段 32 byte | 上位机固定读取版本信息 |
| `0xC008` 起 | `RS485_ADDR_EVENT_RECORD` | `FLASH_STORAGE_LOG_RECORD_COUNT` = 100 | Flash event record | 每条日志 2 byte，正好一个寄存器 |
| `0xD000` 起 | `RS485_ADDR_RO_START0` | `RS485_RO_TOTAL_WORDS` = 114 | 综合实时只读 buffer | 详见下表 |

## `0xD000` 实时只读窗口

`Sci_ACK_0x03_ReadRegs_Data()` 先从 `&g_stCellInfoReport.u16VCell[0]` 连续输出 63 个 word。也就是说 `struct stCell_Info` 的字段顺序就是协议顺序。

| 地址 | word offset | 字段/来源 | 单位/编码 |
| --- | --- | --- | --- |
| `0xD000..0xD01F` | 0..31 | `g_stCellInfoReport.u16VCell[0..31]` | mV；`[29/30]` 可能被 `DISP_VBAT_AND_TEMP_` 复用 |
| `0xD020` | 32 | `u16VCellMax` | mV |
| `0xD021` | 33 | `u16VCellMin` | mV |
| `0xD022` | 34 | `u16VCellMaxPosition` | 1-based position |
| `0xD023` | 35 | `u16VCellMinPosition` | 1-based position |
| `0xD024` | 36 | `u16VCellDelta` | mV |
| `0xD025` | 37 | `u16VCellTotle` | `V * 100` |
| `0xD026..0xD02F` | 38..47 | `u16Temperature[0..9]` | `(C + 40) * 10` |
| `0xD030` | 48 | `u16TempMax` | `(C + 40) * 10` |
| `0xD031` | 49 | `u16TempMin` | `(C + 40) * 10` |
| `0xD032` | 50 | `u16Ichg` | `A * 10` |
| `0xD033` | 51 | `u16IDischg` | `A * 10` |
| `0xD034` | 52 | `SocElement.u16Soc` | percent |
| `0xD035` | 53 | `SocElement.u16Soh` | percent |
| `0xD036` | 54 | `SocElement.u16CapacityNow` | `Ah * 100` |
| `0xD037` | 55 | `SocElement.u16CapacityFull` | `Ah * 100` |
| `0xD038` | 56 | `SocElement.u16CapacityFactory` | `Ah * 100` |
| `0xD039` | 57 | `SocElement.u16Cycle_times` | count |
| `0xD03A` | 58 | `unMdlFault_First.all` | bit mask |
| `0xD03B` | 59 | `unMdlFault_Second.all` | bit mask |
| `0xD03C` | 60 | `unMdlFault_Third.all` | bit mask |
| `0xD03D` | 61 | `u16BalanceFlag1` | bit mask |
| `0xD03E` | 62 | `u16BalanceFlag2` | bit mask |
| `0xD100` | 63 | RTC year/month | high byte = year, low byte = month |
| `0xD101` | 64 | RTC day/hour | high byte = day, low byte = hour |
| `0xD102` | 65 | RTC minute/second | high byte = minute, low byte = second |
| `0xD103..0xD106` | 66..69 | reserved zero | 0 |
| `0xD107..0xD108` | 70..71 | latest `Fault_record_Third2` packed words | 每 word 打包两个历史 byte |
| `0xD109..0xD114` | 72..83 | `System_ErrFlag` 连续字节打包 | 每 word 两个 byte |
| `0xD115` | 84 | `SystemRuntime_GetStatusSnapshot()` low word | OPEN=0 时部分 bit 取反兼容 |
| `0xD116` | 85 | `SystemRuntime_GetStatusSnapshot()` high word | bit mask |
| `0xD117` | 86 | `SystemFeature_GetMask()` low word | bit mask |
| `0xD118` | 87 | `SystemFeature_GetMask()` high word | bit mask |
| `0xD119..0xD120` | 88..95 | reserved zero | 8 words |
| `0xD200` | 96 | `BKP[FAULT_BKP_REASON_REG]` | fault reason |
| `0xD201` | 97 | `BKP[FAULT_BKP_REASON_INV_REG]` | inverse snapshot |
| `0xD202..0xD211` | 98..113 | SOC_TEST compatibility padding | 16 zero words |

注意：

- `0xD100` 和 `0xD200` 不是独立数组，而是映射到同一个 114 word 综合 buffer 的不同 offset。
- 直接改 `struct stCell_Info` 字段顺序会改变 `0xD000` 协议顺序。
- `u16VCell[29]`/`u16VCell[30]` 当前是否继续复用为 Type-C/ADC 总压字段，必须人工确认。

## SCI 写寄存器窗口

### `0x06` 单寄存器命令

| 地址 | 宏 | 当前处理函数 | 数据要求 | 副作用 |
| --- | --- | --- | --- | --- |
| `0x1000` | `RS485_CMD_ADDR_RESET_CALIB_COEF` | `Sci_WrReg_0x06_Reset_CalibCoef()` | 未实现 | 当前空函数 |
| `0x1001` | `RS485_CMD_ADDR_RESET_PROTECT_RECORD` | `Sci_WrReg_0x06_Reset_ProtectRecord()` | `0x0001` | 清 `Fault_record_Third2`、故障 flag |
| `0x1002` | `RS485_CMD_ADDR_RESET_PROTECT_ELEMENT` | `Sci_WrReg_0x06_Reset_ProtectElement()` | `0x0001` | 恢复保护默认值，保存 Flash，`InitData_SOC()` |
| `0x1003` | `RS485_CMD_ADDR_RESET_OTHER_CANADD` | `Sci_WrReg_0x06_Reset_OtherCanAdd()` | `0x0001` | 恢复 `OtherElement` 默认值，保存 Flash，应用副作用 |
| `0x1005` | `RS485_CMD_ADDR_SET_ONCE_SOC` | `Sci_WrReg_0x06_SetSocOnce()` | `0..100` | `SOC_RequestSetOnce()` |
| `0x1006` | `RS485_CMD_ADDR_RESET_AFE_PARAMETERS` | `Sci_WrReg_0x06_Reset_AFE_Parameters()` | 需继续展开 | 重置 AFE 参数 |
| `0x1007` | `RS485_CMD_ADDR_RESET_EVENT_RECORD` | `Sci_WrReg_0x06_Reset_EventRecord()` | 需继续展开 | 重置事件记录 |
| `0x1102` | `RS485_CMD_ADDR_SYSTEM_FUNCTION_ON` | `Sci_WrReg_0x06_BMS_FunctionON()` | supported function id | 打开系统功能；id=7 启动老化；id=0x0A 请求 `DEEP_MODE` |
| `0x1103` | `RS485_CMD_ADDR_SYSTEM_FUNCTION_OFF` | `Sci_WrReg_0x06_BMS_FunctionOFF()` | supported function id | 关闭系统功能；id=7 停止老化 |

`Sci_BmsFunctionIdIsSupported()` 当前支持 function id：`1, 2, 3, 5, 7, 8, 9, 10, 11`。

### `0x10` 多寄存器写

| 地址范围/起点 | 当前处理函数 | 校验 | 保存 | 副作用 |
| --- | --- | --- | --- | --- |
| AFE 参数区 | `Sci_WrRegs_0x10_AFE_Parameters()` | AFE 参数自身校验 | AFE 参数 Flash | 可能置 AFE 参数写回 |
| `0x2100..0x2100+64` | `Sci_WrRegs_0x10_Protect()` | `PRT_E2ROMParas` min/max | `EEPROM_SaveRWParametersToFlash()` | 部分范围触发 `InitData_SOC()` |
| `0x2300..0x2300+31` | `Sci_WrRegs_0x10_OtherElement()` | `OtherElement_min/max` | `EEPROM_SaveRWParametersToFlash()` | 触发 AFE/SOC/SeriesNum/采样电阻副作用 |
| 校准偶数起点 | `Sci_WrRegs_0x10_CalibCoef()` | 必须从 K 起点成对 | 校准 Flash | 更新 K/B |
| `0x2200` | `Sci_WrRegs_0x10_SocTable()` | 无 | 无 | 当前直接负应答 `RS485_ERROR_CMD_INVALID` |
| RTC 起点 | `Sci_WrRegs_0x10_RTC()` | 无 | 无 | 当前空函数 |
| `0xFFF0..0xFFF2` | `Sci_WrRegs_0x10_SN_Version()` | 按起始地址选择 SN/HW/SW | 只写 RAM 结构，是否持久化需继续确认 | 修改 `ProductionInfor` |
| `0xFFFD` | `Sci_WrRegs_0x10_FlashConnect()` | word count 必须为 1 | 通过 `AppUpgrade_RequestIap()` | 成功后 `u8FlashUpdateE2PROM=1`，TX 完成后置 `u8FlashUpdateFlag=1` |

## CAN APP 协议

### 基本帧

| 项 | 当前值 |
| --- | --- |
| 命令 StdId 低位 | `FEIDAO_CAN_APP_CMD_ID = 0x60` |
| ACK StdId 低位 | `FEIDAO_CAN_APP_ACK_ID = 0x61` |
| 实际 StdId | `CAN_ADRESS_STD_ID << 7` 加低位 ID |
| 命令头 | Data[0]=`0xA5`, Data[1]=`0x5A` |
| ACK 头 | Data[0]=`0x5A`, Data[1]=`0xA5` |
| CRC | Data[6..7] = `Sci_CRC16RTU(data, 6)` |
| 命令队列 | `FEIDAO_CAN_APP_CMD_QUEUE_SIZE = 4` |
| 读块最大 word | `FEIDAO_CAN_APP_READ_BLOCK_MAX_WORDS = 120` |

### 命令表

| 命令 | 值 | 参数 | 行为 | ACK value |
| --- | --- | --- | --- | --- |
| `GET_STATUS` | `0x01` | 无 | 读 `g_stCellInfoReport.SocElement.u16Soc/u16Soh` | SOC/SOH 限幅到 0..100 |
| `ENTER_IAP` | `0x02` | Data[3]=`0xC3`, Data[4]=`0x3C`, Data[5]=CAN 地址 | `AppUpgrade_RequestIap()`，延迟置 `u8FlashUpdateFlag` | `0x08/0x48` |
| `READ_REG` | `0x03` | Data[3..4]=地址 | `Sci_HostReadWords(addr,1)` | word high/low |
| `WRITE_PREP` | `0x04` | Data[3..4]=地址，Data[5]=高字节 | 缓存待写地址与高字节 | 地址 high/low |
| `WRITE_COMMIT` | `0x05` | Data[3..4]=地址，Data[5]=低字节 | 地址匹配后 `Sci_HostWriteWords(addr, value,1)` | status |
| `READ_BLOCK` | `0x06` | Data[3..4]=地址，Data[5]=word count | `Sci_HostReadWords()` 后启动分包 | count/0 |
| `AGING_START` | `0x07` | guard/action/id | `FactoryAging_StartByHost()` | aging state/remaining hours |
| `AGING_STOP` | `0x08` | guard/action/id | `FactoryAging_StopByHost()` | aging state/remaining hours |
| `AGING_RESET_TIME` | `0x09` | guard/action/id | `FactoryAging_ResetTimeByHost()` | aging state/remaining hours |
| `AGING_SET_HOURS` | `0x0A` | guard/hours/id | `FactoryAging_SetDurationHoursByHost()` | aging state/remaining hours |
| `READ_BLOCK_DATA` | `0x86` | 固件返回分包 | Data[3]=seq, Data[4..5]=word | 分包数据 |

### ACK 状态

| 状态 | 值 | 来源 |
| --- | --- | --- |
| `FEIDAO_CAN_APP_ACK_OK` | `0x00` | 成功 |
| `FEIDAO_CAN_APP_ACK_BAD_CMD` | `0x01` | 未知命令 |
| `FEIDAO_CAN_APP_ACK_BAD_PARAM` | `0x02` | 地址/数据/只读只写等参数错误 |
| `FEIDAO_CAN_APP_ACK_FLASH_ERR` | `0x05` | `AppUpgrade_RequestIap()` 失败 |
| `FEIDAO_CAN_APP_ACK_NO_PERMISSION` | `0x07` | SCI host write/read 返回无权限 |
| `FEIDAO_CAN_APP_ACK_BMS_ERROR` | `0x08` | 其他 BMS 错误或老化操作失败 |

## CAN 与 SCI 的耦合

| CAN 行为 | 调用 | 结果 |
| --- | --- | --- |
| 读单寄存器 | `Sci_HostReadWords(addr,1,&value)` | 与 Modbus 读窗口完全共享 |
| 读块 | `Sci_HostReadWords(addr,count,s_app.read_block_words)` | 与 Modbus 读窗口完全共享，最多 120 words |
| 写单寄存器 | `Sci_HostWriteWords(addr,&value,1)` | 对 `<0x2000` 的单 word 写走 `0x06`；其他走 `0x10` |
| IAP | `AppUpgrade_RequestIap()` | CAN 和 SCI 都能请求升级 |
| 老化 | `FactoryAging_*ByHost()` | CAN 命令与系统功能 id=7 均影响老化 |

重构含义：

- 修改 `Sci_HostReadWords()`/`Sci_HostWriteWords()` 会同步改变 CAN APP。
- 修改 Modbus 寄存器权限、地址、错误码，也会改变 CAN APP ACK 状态。
- CAN 不是单独协议栈，它是“CAN 帧封装 + Modbus 寄存器桥 + 专用老化/IAP命令”。

## 重构前禁止改变项

| 禁止项 | 原因 |
| --- | --- |
| 改 `0xD000` 字段顺序或长度 | 上位机实时监控依赖 |
| 改 `0xC002` 地址或 48 word 长度 | SN/HW/SW 显示依赖 |
| 改 CAN `0x60/0x61` ID | CAN 工具依赖 |
| 改 `ENTER_IAP` ACK `0x08/0x48` 或 20 tick 延迟 | 升级流程依赖 |
| 改 `Sci_HostWriteWords()` 对 `<0x2000` 单 word 写走 `0x06` 的规则 | CAN 写命令依赖 |
| 删除 `0x2200` 读窗口 | 当前虽然写负应答，但读仍输出 SOC 表和 RTC |

## 需人工确认项

| 项 | 问题 |
| --- | --- |
| `0xD000` 高索引电芯字段 | `u16VCell[29/30]` 是否仍被上位机读取为 Type-C 电流/ADC 总压 |
| `0xC000` LCD 窗口 | 当前 5 word 具体含义是否仍使用 |
| `0xC001` fault history 窗口 | 上位机是否仍按 70 word 读取三级保护历史 |
| `0x2200` SOC 表写 | 当前写负应答是否符合需求，还是遗漏实现 |
| RTC 写函数 | `Sci_WrRegs_0x10_RTC()` 当前空函数是否符合需求 |
| SN/HW/SW 写持久化 | `Sci_WrRegs_0x10_SN_Version()` 当前只写 RAM 结构，是否还需要保存到 Flash |
| CAN 周期帧 | 本文未逐帧冻结，需要继续展开 `CanFeidaoFrames.c` |
