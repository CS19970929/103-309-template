# 公共数据契约与单位冻结表

状态：部分验证。本文基于当前源码整理跨模块共享数据契约，作为后续重构前确认资料，不修改源码。

## 目录

- [范围](#范围)
- [`g_stCellInfoReport` 协议契约](#g_stcellinforeport-协议契约)
- [`OtherElement` 参数契约](#otherelement-参数契约)
- [SOC 数据流契约](#soc-数据流契约)
- [系统状态与错误契约](#系统状态与错误契约)
- [通信与升级标志](#通信与升级标志)
- [低功耗共享状态](#低功耗共享状态)
- [重构前禁止改变项](#重构前禁止改变项)
- [需人工确认项](#需人工确认项)

## 范围

| 数据 | 定义/主要文件 | 为什么是公共契约 |
| --- | --- | --- |
| `g_stCellInfoReport` | `Sci_Upper.c`, `Sci_Upper.h` | 直接映射 `0xD000`，同时被 CAN/SOC/LED/日志/低功耗读取 |
| `OtherElement` | `DataDeal.c`, `DataDeal.h` | 参数区 `0x2300`，影响 AFE/SOC/睡眠/采样电阻/串数 |
| `SOC_Enhance_Element` | `SocEnhance.c`, `SocEnhance.h` | SOC 内部状态与发布源 |
| `System_ErrFlag` | `System_Monitor.c`, `System_Monitor.h` | 通信错误/AFE 错误/系统错误，被协议和保护逻辑读取 |
| `s_system_status/s_system_onoff_func` | `System_Monitor.c` | 输出到 `0xD000` 状态/功能位 |
| `u8FlashUpdateFlag/u8FlashUpdateE2PROM` | `Sci_Upper.c`, `Flash.c` 等 | SCI/CAN/IAP/低功耗共享升级状态 |
| `g_stLowPowerRtcStatus` | `rtc_sleep.c`, `rtc_sleep.h` | 低功耗状态机公共状态 |

## `g_stCellInfoReport` 协议契约

定义位置：`103 + 309/Project/Source/Sci_Upper.h`
实例位置：`103 + 309/Project/Source/Sci_Upper.c`

### 字段表

| 字段 | word offset in `0xD000` | 单位/编码 | 主要写入 | 主要读取 | 冻结建议 |
| --- | --- | --- | --- | --- | --- |
| `u16VCell[0..31]` | 0..31 | mV；`61001` 代表无效/未使用电芯 | `DataLoad_CellVolt()`；`new_todo_logi()` 写 `[29/30]` | SCI `0xD000`，CAN，调试 | 数组长度和顺序不改 |
| `u16VCellMax` | 32 | mV | `DataLoad_CellVoltMaxMinFind()` | SOC、CAN、低功耗、协议 | 单位不改 |
| `u16VCellMin` | 33 | mV | `DataLoad_CellVoltMaxMinFind()` | SOC、低功耗、CAN、协议 | 单位不改 |
| `u16VCellMaxPosition` | 34 | 1-based position | `DataLoad_CellVoltMaxMinFind()` | 协议/调试 | 不改 |
| `u16VCellMinPosition` | 35 | 1-based position | `DataLoad_CellVoltMaxMinFind()` | 协议/调试 | 不改 |
| `u16VCellDelta` | 36 | mV | `DataLoad_CellVoltMaxMinFind()` | 保护/SOC/协议 | 不改 |
| `u16VCellTotle` | 37 | `V * 100` | `DataLoad_CellVoltMaxMinFind()` | CAN、SOC Type-C 折算、协议 | 单位必须显式标注 |
| `u16Temperature[0..9]` | 38..47 | `(C + 40) * 10` | `DataLoad_Temperature()` | 保护、MOS、CAN、协议 | 单位不改 |
| `u16TempMax` | 48 | `(C + 40) * 10` | `DataLoad_TemperatureMaxMinFind()` | CAN/协议 | 不改 |
| `u16TempMin` | 49 | `(C + 40) * 10` | `DataLoad_TemperatureMaxMinFind()` | 协议 | 不改 |
| `u16Ichg` | 50 | `A * 10` | `DataLoad_Current()` | SOC、低功耗、CAN、协议 | 单位不改 |
| `u16IDischg` | 51 | `A * 10` | `DataLoad_Current()` | SOC、低功耗、CAN、协议 | 单位不改 |
| `SocElement.u16Soc` | 52 | percent | `SOC_PublishReportData()` | LED、CAN、协议 | 输出范围应保持 0..100 |
| `SocElement.u16Soh` | 53 | percent | `SOC_PublishReportData()` | CAN、协议 | 不改 |
| `SocElement.u16CapacityNow` | 54 | `Ah * 100` | `SOC_PublishReportData()` | CAN、协议 | 单位不改 |
| `SocElement.u16CapacityFull` | 55 | `Ah * 100` | `SOC_PublishReportData()` | 协议/调试 | 单位不改 |
| `SocElement.u16CapacityFactory` | 56 | `Ah * 100` | `SOC_PublishReportData()` | CAN、协议 | 单位不改 |
| `SocElement.u16Cycle_times` | 57 | count | `SOC_PublishReportData()` | CAN、协议 | 不改 |
| `unMdlFault_First.all` | 58 | bit mask | AFE/保护/故障模块 | 协议/调试 | bit 位不改 |
| `unMdlFault_Second.all` | 59 | bit mask | AFE/保护/故障模块 | 协议/调试 | bit 位不改 |
| `unMdlFault_Third.all` | 60 | bit mask | `FaultWarnRecord2()`、AFE 监控等 | 协议、CAN、低功耗、LED、日志 | bit 位不改 |
| `u16BalanceFlag1` | 61 | bit mask | AFE/均衡逻辑 | 协议 | 未完全确认 |
| `u16BalanceFlag2` | 62 | bit mask | AFE/均衡逻辑 | 协议 | 未完全确认 |

### `u16VCell[29/30]` 兼容复用

当前 `conf.h` 定义 `DISP_VBAT_AND_TEMP_`，`new_todo_logi()` 内执行：

| 字段 | 当前写入 | 单位/含义 | 风险 |
| --- | --- | --- | --- |
| `u16VCell[29]` | `SOC_GetTypeCBatEquivCurrentA10()` | Type-C 输出折算到电池侧的等效电流，`A * 10` | 字段名仍是电芯电压，容易被误认为 mV |
| `u16VCell[30]` | `ADC_GetVbatMilliVolt()` | ADC 总压 mV | 与 `u16VCellTotle`/AFE 总压概念重叠 |

建议：未确认上位机是否读取前，不删除、不改地址；只在文档和协议表标注为兼容字段。

## `OtherElement` 参数契约

定义位置：`103 + 309/Project/Source/DataDeal.h`
实例位置：`103 + 309/Project/Source/DataDeal.c`
协议窗口：`0x2300..0x231F`

### 字段与 offset

| offset | 字段 | 单位/含义 | 影响模块 |
| --- | --- | --- | --- |
| 0 | `u16Balance_OpenVoltage` | mV | 均衡/AFE 参数写回 |
| 1 | `u16Balance_OpenWindow` | mV | 均衡 |
| 2 | `u16Balance_CloseWindow` | mV | 均衡 |
| 3..7 | `u16Balance_Res1..Res5` | reserved | 兼容保留 |
| 8 | `u16CS_Cur_CHGmax` | `A * 10` | 短路/电流保护 |
| 9 | `u16CS_Cur_DSGmax` | `A * 10` | 短路/电流保护 |
| 10 | `u16CBC_DelayT` | `us * 10` 或近似表值 | AFE 短路延时 |
| 11 | `u16CBC_Cur_DSG` | `A * 10` | AFE 短路电流 |
| 12 | `u16Soc_TableSelect` | enum | SOC 表选择 |
| 13 | `u16Password_Always` | 未确认 | 历史字段 |
| 14 | `u16CurLimit_Vdelta` | mV | 限流/压差 |
| 15 | `u16CurLimit_Cur` | `A * 10` | 限流 |
| 16 | `u16Sleep_VNormal` | mV | 正常睡眠阈值 |
| 17 | `u16Sleep_TimeNormal` | min | 正常睡眠时间 |
| 18 | `u16Sleep_Vlow` | mV | 低压 deep 阈值 |
| 19 | `u16Sleep_TimeVlow` | min | 低压 deep 延时 |
| 20 | `u16Sleep_VirCur_Chg` | `A * 10` | 虚拟充电电流 |
| 21 | `u16Sleep_VirCur_Dsg` | `A * 10` | 虚拟放电电流 |
| 22 | `u16Sleep_RTC_WakeUpTime` | min | RTC 唤醒周期 |
| 23 | `u16Sleep_TimeRTC` | min | RTC 睡眠时间 |
| 24 | `u16Soc_Ah` | `10 * Ah` | SOC 容量配置 |
| 25 | `u16Soc_Cycle_times` | count | SOC 循环次数初始/配置 |
| 26 | `u16Soc_V_100` | mV | SOC 满电电压 |
| 27 | `u16Soc_V_0` | mV | SOC 空电电压 |
| 28 | `u16Sys_SeriesNum` | N | 串数、总压阈值、CAN |
| 29 | `u16Sys_CS_Res` | mΩ | 电流换算 |
| 30 | `u16Sys_CS_Res_Num` | N | 电流换算比例 |
| 31 | `u16Sys_PreChg_Time` | s | 预充时间 |

### 写入副作用

| offset 范围 | 副作用 | 原因 |
| --- | --- | --- |
| 0..7 | `AFE_PARAM_WRITE_Flag=1` | 均衡参数需要写回 AFE |
| 8..15 | `AFE_PARAM_WRITE_Flag=1` | 短路/电流相关参数影响 AFE |
| 24..27 | `InitData_SOC()` + `SOC_RequestCapacityReset()` | SOC 容量/电压配置变化后必须重载并重置容量 |
| 28..31 | `AFE_PARAM_WRITE_Flag=1`，更新 `SeriesNum` 和 `g_u32CS_Res_AFE` | 串数/采样电阻影响 AFE 和电流换算 |

## SOC 数据流契约

### 输入

| 来源 | 字段 | 单位 | 进入 SOC 前处理 |
| --- | --- | --- | --- |
| 采样报告 | `g_stCellInfoReport.u16VCellMax` | mV | 直接传给 `SOC_UpdateSampleData()` |
| 采样报告 | `g_stCellInfoReport.u16VCellMin` | mV | 直接传给 `SOC_UpdateSampleData()` |
| 采样报告 | `g_stCellInfoReport.u16Ichg` | `A * 10` | 与 Type-C 等效放电抵消后进入 SOC |
| 采样报告 | `g_stCellInfoReport.u16IDischg` | `A * 10` | 加上 Type-C 等效放电后进入 SOC |
| Type-C ADC | `ADC_GetTypeCOutCurrentMilliAmp()` | mA | `SOC_GetTypeCBatEquivCurrentA10()` 折算为电池侧 `A * 10` |
| 参数 | `OtherElement.u16Soc_Ah` | `10 * Ah` | `InitData_SOC()` 加载到 `SOC_Enhance_Element.u16_SOC_Ah` |
| 参数 | `OtherElement.u16Soc_Cycle_times` | count | 加载到 SOC 配置快照 |
| 参数 | `OtherElement.u16Soc_V_100` | mV | 满电校准阈值 |
| 参数 | `OtherElement.u16Soc_V_0` | mV | 空电校准阈值 |

### 内部状态与输出

| 字段 | 所在结构 | 含义 | 发布到 |
| --- | --- | --- | --- |
| `u16_VCellMax/Min` | `SOC_Enhance_Element` | 最新 SOC 计算电压样本 | SOC 内部算法 |
| `u16_Ichg/u16_Idsg` | `SOC_Enhance_Element` | 最新 SOC 计算电流样本，已处理 Type-C 等效电流 | SOC 内部算法 |
| `u8_SOC/u8_SOH` | `SOC_Enhance_Element` | 内部计算输出 | `g_stCellInfoReport.SocElement` |
| `u16_CapacityNow/Full/Factory` | `SOC_Enhance_Element` | 容量输出，`Ah * 100` | `g_stCellInfoReport.SocElement` |
| `u16_Cycle_times` | `SOC_Enhance_Element` | 循环次数 | `g_stCellInfoReport.SocElement` |
| `u16_RefreshData_Flag` | `SOC_Enhance_Element` | 命令选择：2=capacity reset，3=set once SOC | `soc_handle_command()` 消费 |

关键约束：

- `g_stCellInfoReport.u16IDischg` 不直接包含 Type-C 等效放电；Type-C 只在进入 SOC 计算时折算叠加。
- `SOC_PublishReportData()` 是 SOC 输出到协议报告的边界。
- `App_SOC()` 只有在 `AfeCurrent_GetSeq()` 变化时才更新 SOC 算法；否则只发布现有数据。

## 系统状态与错误契约

| 数据 | 来源 | 输出位置 | 影响 |
| --- | --- | --- | --- |
| `System_ErrFlag` | `System_Monitor.c` | `0xD109..0xD114` 按 byte 打包 | AFE 错误、通信错误、温度断线等状态输出 |
| `SystemRuntime_GetStatusSnapshot()` | `System_Monitor.c` | `0xD115..0xD116` | 上位机系统状态 |
| `SystemFeature_GetMask()` | `System_Monitor.c` | `0xD117..0xD118` | 上位机功能开关状态 |
| `SystemFeature_SetById()` | `System_Monitor.c` | SCI/CAN 功能命令触发 | id=7 关联老化；id=10 请求 deep sleep；id=11 SOC zero overlay |

风险点：

| 风险 | 说明 |
| --- | --- |
| `System_ErrFlag` 被按连续字节读取 | 改结构体字段顺序会改变协议输出 |
| `OPEN==0` 时 status low word 部分 bit 取反 | 兼容逻辑不直观，不能直接删除 |
| function id 是协议值 | 不能随意重排 `SystemFeature_SetById()` 的 bit 含义 |

## 通信与升级标志

| 变量/状态 | 写入者 | 读取者 | 含义 |
| --- | --- | --- | --- |
| `u8FlashUpdateFlag` | SCI/CAN IAP、TX 完成路径 | 低功耗 blocker、升级流程 | 非 0 表示请求进入升级 |
| `u8FlashUpdateE2PROM` | `Sci_WrRegs_0x10_FlashConnect()` | `Sci_PortFinishTx()`、低功耗 blocker | 等待当前 SCI ACK 发完后置升级 flag |
| `sys_time.can_rcv_cnt` | `USB_LP_CAN1_RX0_IRQHandler()` | `Can_IsBusy()` | CAN 接收活动计数 |
| `sys_time.last_ext_comm_cnt_can` | `Can_IsBusy()` | `Can_PeekBusy()/Can_IsBusy()` | CAN 低功耗活动快照 |
| `s_sleep.ext_comm` | `USART RXNE` | `LP_GetBlockReason()` | SCI 外部通信活动计数 |

注意：`Can_IsBusy()` 是“消费式 busy 判断”，不是纯读快照。

## 低功耗共享状态

| 字段 | 单位/含义 | 影响 |
| --- | --- | --- |
| `g_stLowPowerRtcStatus.mode` | `NORMAL/HICCUP/DEEP/NO_SLEEP` | 决定 `rtc_sleep()` 进入 reset-sleep 还是 STOP loop |
| `g_stLowPowerRtcStatus.block` | blocker bitmask | debug 和低功耗原因定位 |
| `g_stLowPowerRtcStatus.sleep` | seconds | HICCUP 累计睡眠秒，影响 SOC RTC rest 补偿 |
| `g_stLowPowerRtcStatus.last` | seconds | 最近一次 HICCUP 结束睡眠秒 |
| `g_stLowPowerRtcStatus.cycles` | count | HICCUP RTC 唤醒轮数 |

低功耗直接读取的数据：

| 数据 | 单位 | 用途 |
| --- | --- | --- |
| `g_stCellInfoReport.u16VCellMin` | mV | 低压/极低压 deep、应急唤醒 |
| `g_stCellInfoReport.u16Ichg` | `A * 10` | 充电 blocker、deep 允许条件 |
| `g_stCellInfoReport.u16IDischg` | `A * 10` | 放电 blocker |
| `OtherElement.u16Sleep_Vlow` | mV | 低压 deep 阈值 |
| `OtherElement.u16Sleep_TimeVlow` | min | 低压 deep 延时 |
| `g_stCellInfoReport.unMdlFault_Third.all` | bitmask | fault blocker |

## 重构前禁止改变项

| 禁止项 | 原因 |
| --- | --- |
| 改 `struct stCell_Info` 字段顺序 | 直接改变 `0xD000` 协议 |
| 改 `u16Ichg/u16IDischg` 单位 | SOC、低功耗、CAN、协议都会受影响 |
| 把 Type-C 等效电流直接写入 `u16IDischg` | 当前只在 SOC 输入层叠加，协议显示仍是采样放电电流 |
| 改 `OtherElement` 字段顺序 | 直接改变 `0x2300` 参数协议 |
| 删除 `OtherElement` 写入副作用 | 会导致 AFE/SOC/采样电阻配置不即时生效 |
| 改 `System_ErrFlag` 字段顺序 | 直接改变 `0xD109..0xD114` 输出 |
| 把 `Can_IsBusy()` 当纯查询复用 | 会消费 CAN 低功耗通信计数 |

## 需人工确认项

| ID | 问题 | 建议 |
| --- | --- | --- |
| DATA-Q1 | `u16VCell[29/30]` 兼容复用是否仍保留？ | 若保留，命名和文档标注兼容用途；若删除，需上位机确认 |
| DATA-Q2 | `u16VCellTotle` 是否继续使用 `V * 100` 而不是 mV？ | 保持协议兼容，CAN 内部按需要乘 10 |
| DATA-Q3 | `rtc_sleep_port.c` 中 `GetChargeCurrentMa` 命名是否允许后续改为 `A10`？ | 建议等价重命名，降低误读 |
| DATA-Q4 | `OtherElement.u16Password_Always` 和 reserved 字段是否仍有上位机兼容意义？ | 未确认前不删字段 |
| DATA-Q5 | `SystemFeature` id 1/2/3/5/7/8/9/10/11 的用户可见含义是否有文档？ | 需要补功能 id 表 |
| DATA-Q6 | SN/HW/SW 写入是否需要持久化？ | 当前 `Sci_WrRegs_0x10_SN_Version()` 只写 RAM 结构，需确认 |
