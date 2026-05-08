# System_Monitor 模块梳理

日期：2026-04-25

## 1. 分析范围

本次只做静态代码梳理，未修改源码、未编译验证。分析范围主要包括：

- `103 + 309/Project/Source/System_Monitor.c`
- `103 + 309/Project/Source/System_Monitor.h`
- `103 + 309/Project/Source/*.c`
- `103 + 309/Project/Source/*.h`
- `103 + 309/Project/Source/conf/conf.h`

判断依据：

- 变量是否被外部模块读取。
- 变量是否被外部模块写入。
- 变量是否只在 `System_Monitor.c` 内初始化、置位、清零。
- 变量是否只作为通信协议上传位存在。
- 相关业务入口是否仍在主循环或编译宏中启用。

需要特别注意：`System_Monitor` 中多个结构体/联合体会通过 `SystemStatus.all`、`System_OnOFF_Func.all` 或连续内存方式上传给上位机。即使某些字段已经没有业务逻辑，也可能仍是通信协议占位，不能在未确认协议布局前直接删除。

## 2. 模块当前职责

`System_Monitor` 当前不是一个独立运行的监控任务，而是一个全局状态集合模块，主要承担四类职责：

1. 初始化系统功能开关、启动标志、运行状态。
2. 保存系统错误计数/锁存标志。
3. 为 CAN、RS485、日志、低功耗、保护逻辑提供状态查询。
4. 保留一些老项目功能位和协议位。

当前定义的全局对象：

| 全局对象 | 类型 | 当前作用 | 判断 |
| --- | --- | --- | --- |
| `System_ErrFlag` | `struct SYSTEM_ERROR` | 错误标志/错误计数集合 | 部分有效，部分老项目残留 |
| `System_OnOFF_Func` | `union System_OnOFF_Function` | 功能开关集合，上位机可写 | 部分有效，但 EEPROM 恢复链路断裂 |
| `System_OnOFF_Func_StartUpRec` | `union System_OnOFF_Function` | 记录功能是否首次打开，用于重新触发启动初始化 | 基本无效，因为对应启动标志无人读取 |
| `SystemStatus` | `union System_Status` | 系统运行状态集合，部分用于控制，部分用于上传 | 部分有效，部分仅协议占位 |
| `System_Func_StartUp` | `union System_Function_StartUp` | 老的开机初始化流程标志 | 基本废弃 |

初始化入口：

- `InitSystemMonitorData_EEPROM()` 在 `main.c` 的 `InitVar()` 中调用。
- `SystemMonitorResetData_EEPROM()` 当前只声明和定义，`main.c` 中调用被注释。

## 3. 关键链路问题

### 3.1 EEPROM 功能开关读取被注释

`System_OnOFF_Func` 本来应支持从 EEPROM 读取功能开关：

```c
// System_OnOFF_Func.all = (UINT32)ReadEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT);
// System_OnOFF_Func.all |= ((UINT32)ReadEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT + 2)<<16);
```

但当前读取逻辑被注释，实际开机始终使用 `InitSystemMonitorData_EEPROM()` 中的硬编码默认值。

同时，`Sci_Upper.c` 中仍会在上位机打开/关闭功能时写 EEPROM：

- `Sci_WrReg_0x06_BMS_FunctionON()`
- `Sci_WrReg_0x06_BMS_FunctionOFF()`

这导致一个明显断裂：

- 运行中上位机修改功能开关会影响 RAM。
- 代码也会写 EEPROM。
- 但下次重启不会读回 EEPROM，修改不会恢复。

后续需要决定：

1. 恢复 EEPROM 读取功能。
2. 或明确废弃 EEPROM 功能开关，删除写 EEPROM 行为，避免误导。

### 3.2 启动标志无人读取

`System_Func_StartUp` 的多个 bit 会被初始化、置位、清零，例如：

- `b1StartUpFlag_SOC`
- `b1StartUpFlag_Balance`
- `b1StartUpFlag_MOS`
- `b1StartUpFlag_Relay`
- `b1StartUpFlag_Heat`
- `b1StartUpFlag_Cool`

但全项目没有找到任何业务逻辑读取这些 bit 来判断启动是否完成。也就是说，这套“启动初始化状态机”基本已经失效，只剩写入。

### 3.3 主循环中部分业务入口未启用

当前主循环状态：

- `App_CellBalance()` 被注释，均衡主流程不运行。
- `App_MOS_Relay_Ctrl()` 没有实际调用，MOS/继电器统一控制入口不运行。
- `__FUNC__HEAT__` 当前在 `conf.h` 中已定义，热管理入口参与编译。
- `App_Heat_Cool_Ctrl()` 当前只调用 `Heat_Control()`，没有调用 `Cool_Control()`。
- `App_LowPowerProcess()` 当前直接调用 `rtc_sleep()`，老的 `b1OnOFF_RTC` 相关逻辑在 `#if 0` 中，且 `System_OnOFF_Function` 内没有 `b1OnOFF_RTC` 字段。

这会影响对变量有效性的判断：有些字段不是完全没有逻辑，而是对应业务入口已经关闭。

## 4. `System_ErrFlag` 字段梳理

| 字段 | 当前作用 | 外部使用情况 | 建议 |
| --- | --- | --- | --- |
| `u8ErrFlag_Com_AFE1` | AFE1 通信/配置/读写错误 | 被 AFE、休眠、IO 强制关闭、RS485 上传、部分认证逻辑使用 | 保留 |
| `u8ErrFlag_Com_AFE2` | AFE2 错误 | 被 `DataDeal.c` 置位，IO 强制关闭、日志、休眠读取；清除逻辑被注释 | 保留但需修复清除链路 |
| `u8ErrFlag_Com_Can` | CAN 错误计数 | `Can_HDX.c` 会置位，但基本只用于状态查询/上传 | 可保留为上传诊断位 |
| `u8ErrFlag_Com_EEPROM` | EEPROM 通信错误 | IO 强制关闭、休眠判断会读，但有效置位来源不明显 | 需确认 EEPROM 层是否应置位 |
| `u8ErrFlag_Com_SPI` | AFE PF 类异常映射 | `SH367309_Func.c` 会置位，主要用于上传 | 可保留或改名 |
| `u8ErrFlag_Com_Upper` | AFE WDT 类异常映射 | `SH367309_Func.c` 会置位，主要用于上传 | 可保留或改名 |
| `u8ErrFlag_Com_Client` | AFE L0V 类异常映射 | `SH367309_Func.c` 会置位，主要用于上传 | 可保留或改名 |
| `u8ErrFlag_Com_Screen` | 屏幕通信错误 | 未找到有效外部置位/读取 | 疑似废弃 |
| `u8ErrFlag_Com_Wifi` | WiFi 通信错误 | 未找到有效外部置位/读取 | 疑似废弃 |
| `u8ErrFlag_Com_BlueTooth` | 蓝牙通信错误 | 未找到有效外部置位/读取 | 疑似废弃 |
| `u8ErrFlag_Com_App` | APP 通信错误 | 未找到有效外部置位/读取 | 疑似废弃 |
| `u8ErrFlag_CBC_CHG` | 充电短路/过流类错误 | 未找到有效外部置位/读取 | 疑似废弃或未接入 |
| `u8ErrFlag_CBC_DSG` | 放电短路/短路保护错误 | 被 CAN、日志、LED、休眠、AFE 状态同步使用 | 保留 |
| `u8ErrFlag_Store_EEPROM` | EEPROM/Flash 存储错误 | Flash、日志、AFE 配置会置位；IO 强制关闭、休眠、日志读取 | 保留 |
| `u8ErrFlag_HSE` | HSE 时钟错误 | `System_Init.c` 读取影响 TIM3 分频，但未找到有效置位 | 需确认时钟异常检测是否已废弃 |
| `u8ErrFlag_LSE` | LSE 时钟错误 | `RTC.c` 中置位调用被注释 | 疑似废弃 |
| `u8ErrFlag_Vdelta_OVER` | 压差过大错误 | `Fault.c` 会置位/清零，主要用于上传 | 可保留 |
| `u8ErrFlag_Balanced` | 均衡动作记录/错误 | 均衡逻辑会置位，但主循环 `App_CellBalance()` 当前被注释 | 取决于是否恢复均衡 |
| `u8ErrFlag_ADC` | ADC 错误 | 未找到有效外部置位/读取 | 疑似废弃 |
| `u8ErrFlag_SOC_Cail` | SOC 校准/估算错误计数 | `SOC.c` 直接同步 `SOC_Enhance_Element.u16_SOC_CailFaultCnt` | 保留 |
| `u8ErrFlag_Heat` | 加热超时错误 | 热管理启用后会置位；当前宏已开启 | 需上板验证 3 小时超时和恢复路径 |
| `u8ErrFlag_Cool` | 冷却错误 | 未找到有效外部置位/读取 | 疑似废弃 |
| `u8ErrFlag_TempBreak` | 温感断线错误 | `PubFunc.c` 置位/清零，IO、LED、日志读取 | 保留 |
| `u8Res6` | 保留字节 | 无业务作用 | 保留为协议/对齐占位或重命名 |

错误命令枚举中疑似废弃或未接入项：

- `ERROR_SCREEN`
- `ERROR_WIFI`
- `ERROR_BLUETOOTH`
- `ERROR_APP`
- `ERROR_CBC_CHG`
- `ERROR_LSE`
- `ERROR_ADC`
- `ERROR_COOL`
- 对应的 `ERROR_REMOVE_*` 和 `ERROR_STATUS_*`

注意：这些枚举即使不用，也可能影响上位机错误码编号，不建议直接删除，可先标注 deprecated。

## 5. `System_OnOFF_Func` 字段梳理

| 字段 | 当前作用 | 外部使用情况 | 建议 |
| --- | --- | --- | --- |
| `b1OnOFF_Balance` | 均衡功能开关 | 均衡逻辑读取，但主循环 `App_CellBalance()` 当前被注释；上位机可写 | 取决于是否恢复均衡 |
| `b1OnOFF_BMS_Source` | BMS 电源/源控制开关 | 只初始化，未找到业务读取 | 疑似废弃 |
| `b1OnOFF_MOS_Relay` | MOS/继电器总开关 | `IO_Control.c`、`ChargerLoadFunc.c`、`Sci_Upper.c` 使用；但 `App_MOS_Relay_Ctrl()` 当前无调用 | 逻辑残留，需结合实际驱动路径确认 |
| `b1OnOFF_Relay_Rec` | 继电器记录/恢复 | 只初始化注释，未找到业务读取 | 疑似废弃 |
| `b1OnOFF_SOC_Fixed` | SOC 固定为 60 | `SOC.c`、`SocEnhance.c`、上位机写寄存器使用 | 保留，调试/生产功能 |
| `b1OnOFF_Heat` | 加热功能开关 | `Heat_Cool.c` 读取；当前 `__FUNC__HEAT__` 已开启 | 需验证不会绕过充电保护链路 |
| `b1OnOFF_Cool` | 冷却功能开关 | `Cool_Control()` 读取；但 `Cool_Control()` 当前未被主入口调用 | 疑似未完成/未启用 |
| `b1OnOFF_AFE1` | AFE1 功能开关 | 只初始化，未找到业务读取 | 疑似废弃 |
| `b1OnOFF_AFE2` | AFE2 功能开关 | 只初始化，未找到业务读取 | 疑似废弃 |
| `b1OnOFF_Sleep` | 主循环空闲 WFI 开关 | `main.c` 的 `MainLoop_EnterIdleSleep()` 使用 | 保留 |
| `b1OnOFF_SOC_Zero` | SOC 强制为 0 | `SOC.c`、`SocEnhance.c`、上位机写寄存器使用 | 保留，调试/生产功能 |
| `bRcved*` | 保留位 | 无业务作用 | 保留为协议占位 |

当前明显问题：

- 上位机能通过功能编号 1 到 32 修改 `System_OnOFF_Func.all`，但 bitfield 里只有少数字段有业务含义。
- `b1OnOFF_AFE1` / `b1OnOFF_AFE2` 看起来像应控制 AFE 使能，但实际没有任何判断。
- `b1OnOFF_Cool` 默认初始化为 0，`SystemMonitorResetData_EEPROM()` 又写成 1，两处默认值不一致。

## 6. `System_OnOFF_Func_StartUpRec` 梳理

用途注释是：如果某功能开机不打开，后续运行中途打开，则需要初始化，该位为记录位。

当前实际链路：

- `InitSystemMonitorData_EEPROM()` 中复制 `System_OnOFF_Func.all`。
- `Sci_WrReg_0x06_BMS_FunctionON()` 中针对均衡、MOS/继电器、加热、冷却设置对应启动标志。
- 但 `System_Func_StartUp` 没有读取者。

结论：

- `System_OnOFF_Func_StartUpRec` 当前基本没有实际运行效果。
- 若不恢复启动检查状态机，可删除这套逻辑或降级为注释说明。
- 若要恢复，则需要明确每个功能的“初始化完成条件”和“阻止运行条件”。

## 7. `System_Func_StartUp` 字段梳理

| 字段 | 当前使用情况 | 判断 |
| --- | --- | --- |
| `b1StartUpFlag_SOC` | `SOC.c` 初始化完成后清零；无人读取 | 基本废弃 |
| `b1StartUpFlag_Balance` | 上位机首次打开均衡时置 1；无人读取 | 基本废弃 |
| `b1StartUpFlag_Protect` | 只初始化；无人读取 | 废弃 |
| `b1StartUpFlag_MOS` | 上位机首次打开 MOS/继电器时置 1；无人读取 | 基本废弃 |
| `b1StartUpFlag_Relay` | 上位机首次打开 MOS/继电器时置 1；无人读取 | 基本废弃 |
| `b1StartUpFlag_ADC` | 只初始化；无人读取 | 废弃 |
| `b1StartUpFlag_CAN` | 只初始化；无人读取 | 废弃 |
| `b1StartUpFlag_Cool` | 冷却自检清零，上位机首次打开时置 1；无人读取 | 基本废弃 |
| `b1StartUpFlag_Heat` | 加热自检清零，上位机首次打开时置 1；无人读取 | 基本废弃 |
| `b1StartUpFlag_AFE1` | 只定义；无人读取 | 废弃 |
| `b1StartUpFlag_AFE2` | 只定义；无人读取 | 废弃 |
| `b1StartUpFlag_BlueT` | 只初始化；无人读取 | 废弃 |
| `bRcved*` | 保留位 | 占位 |

同时，`enum SYSTEM_FUNC_STARTUP_COMMAND` 和 `StartUp_Status` 当前没有任何外部使用。

## 8. `SystemStatus` 字段梳理

| 字段 | 当前作用 | 外部使用情况 | 建议 |
| --- | --- | --- | --- |
| `b1StartUpBMS` | 老的开机状态标志 | 初始化置 1 后 `main.c` 马上清 0；LED 会读取；`Fault.c` 中相关逻辑在 `#if 0` | 疑似废弃或仅 LED 兼容 |
| `b1Status_MOS_PRE` | 预充 MOS 状态 | 当前有效引用基本在注释中；运行链路未见有效读写 | 疑似废弃 |
| `b1Status_MOS_CHG` | 充电 MOS 状态 | CAN、低功耗、外部驱动控制、AFE 状态同步使用 | 保留 |
| `b1Status_MOS_DSG` | 放电 MOS 状态 | CAN、低功耗、外部驱动控制、AFE 状态同步使用 | 保留 |
| `b1Status_Relay_PRE` | 预充继电器状态 | 只初始化/上传，未见有效业务读写 | 疑似废弃 |
| `b1Status_Relay_CHG` | 充电继电器状态 | 只初始化/上传，未见有效业务读写 | 疑似废弃 |
| `b1Status_Relay_DSG` | 放电继电器状态 | 只初始化/上传，未见有效业务读写 | 疑似废弃 |
| `b1Status_Relay_MAIN` | 主继电器状态 | 只初始化/上传，未见有效业务读写 | 疑似废弃 |
| `b1Status_Heat` | 加热输出状态 | 热管理启用后控制输出；日志、休眠读取 | 取决于是否恢复加热 |
| `b1Status_Cool` | 冷却输出状态 | `Cool_Control()` 内写，但当前冷却控制未被入口调用；日志读取 | 疑似未启用 |
| `b1Status_AFE1` | AFE1 在线/通信状态 | `DataDeal.c` 写，协议上传 | 保留 |
| `b1Status_AFE2` | AFE2 在线/通信状态 | `DataDeal.c` 写，协议上传 | 保留或随 AFE2 裁剪 |
| `b1Status_Balance` | 均衡状态 | 未找到有效业务读写 | 疑似废弃 |
| `b1Status_ToSleep` | 进入休眠状态 | `main.c` 只写 1，未见有效读取 | 疑似废弃 |
| `b1Status_BnCloseIO` | 均衡关闭 IO/强关 MOS 标志 | 有读取，但未找到内部写入来源 | 需确认是否外部调试写入 |
| `b1Status_HeatCloseIO` | 加热强制关闭 IO | 有读取，但未找到内部写入来源 | 需确认是否外部调试写入 |
| `b1Status_SysLimits` | 系统限制/权限 | 未找到有效业务读写 | 疑似废弃 |
| `b1Status_CBCCloseIO` | CBC 强制关闭 IO | 有读取，但未找到内部写入来源 | 需确认是否外部调试写入 |
| `b1Status_DriverExtCtrl` | 外部驱动控制 | 未找到有效业务读写 | 疑似废弃 |
| `b4Status_ProjectVer` | 项目版本位 | `main.c` 写 1，通过状态上传 | 可保留为协议位 |
| `bRcved*` | 保留位 | 无业务作用 | 保留为协议占位 |

## 9. 可优先清理项

建议优先处理“没有协议风险或协议风险低”的内容：

1. 给 `StartUp_Status`、`SYSTEM_FUNC_STARTUP_COMMAND`、`System_Func_StartUp` 加废弃注释，或直接规划删除。
2. 明确 `System_OnOFF_Func_StartUpRec` 是否还有恢复价值；若没有，连同上位机打开功能时设置启动标志的逻辑一起清理。
3. 修复 `ERROR_REMOVE_AFE2` 被注释的问题，或者明确 AFE2 永久不用。
4. 明确 `System_OnOFF_Func` 的 EEPROM 策略：恢复读取或删除写入。
5. 明确主循环是否要恢复 `App_CellBalance()`、`App_MOS_Relay_Ctrl()`、热管理。

暂不建议立即删除的内容：

- `System_ErrFlag` 内字段。
- `SystemStatus` 内字段。
- `System_OnOFF_Func` 内字段。

原因是这些字段可能参与上位机寄存器布局和历史协议。删除或重排 bitfield 会改变通信含义。

## 10. 建议的后续改造路线

### 阶段一：文档和标注

- 保持二进制布局不变。
- 给疑似废弃字段增加 `deprecated` 注释。
- 整理上位机寄存器与 bit 位映射表。
- 明确每个状态位是“控制位”、“状态位”、“错误位”还是“仅上传位”。

### 阶段二：修复明显断裂链路

- 恢复或删除 EEPROM 功能开关读取。
- 修复 AFE2 错误清除。
- 检查 HSE/LSE 错误是否还有检测入口。
- 确认 `__FUNC__HEAT__`、均衡、MOS 控制入口是否按当前项目需求启用。

### 阶段三：结构收敛

建议最终拆成三个更明确的模块：

- `SystemFault`：错误计数和错误状态查询。
- `SystemFeature`：功能开关和 EEPROM 持久化。
- `SystemRuntimeStatus`：运行状态和通信上传快照。

这样可以避免当前 `System_Monitor` 同时混合“错误、功能开关、启动状态、协议上传、运行状态”的问题。

## 11. 初步结论

当前最像老项目残留、优先级最高的废弃候选：

- `StartUp_Status`
- `SYSTEM_FUNC_STARTUP_COMMAND`
- `System_Func_StartUp`
- `System_OnOFF_Func_StartUpRec`
- `b1OnOFF_BMS_Source`
- `b1OnOFF_Relay_Rec`
- `b1OnOFF_AFE1`
- `b1OnOFF_AFE2`
- `b1Status_Relay_PRE`
- `b1Status_Relay_CHG`
- `b1Status_Relay_DSG`
- `b1Status_Relay_MAIN`
- `b1Status_Balance`
- `b1Status_ToSleep`
- `b1Status_SysLimits`
- `b1Status_DriverExtCtrl`

但在清理前，应先确认 RS485/CAN 上位机协议是否依赖这些字段的位置。对于协议相关字段，推荐先改注释和文档，不直接改结构布局。
