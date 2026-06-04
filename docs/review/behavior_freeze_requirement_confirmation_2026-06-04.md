# 第一阶段外部行为冻结与需求确认

状态：部分验证。本文只基于当前源码和 `docs/code_flow_analysis/` 文档整理确认项，不修改业务源码，不提交 git。

## 目录

- [分析范围](#分析范围)
- [核心判断](#核心判断)
- [需求确认总表](#需求确认总表)
- [协议与状态冻结清单](#协议与状态冻结清单)
- [建议执行顺序](#建议执行顺序)
- [必须人工确认的问题](#必须人工确认的问题)
- [未确认项](#未确认项)

## 分析范围

本轮从以下文档和源码继续推进：

| 类型 | 文件 |
| --- | --- |
| 主流程文档 | `docs/code_flow_analysis/01_main_startup_flow.md` |
| 初始化文档 | `docs/code_flow_analysis/02_init_flow.md` |
| 主循环文档 | `docs/code_flow_analysis/03_main_loop_flow.md` |
| 中断文档 | `docs/code_flow_analysis/04_interrupt_async_flow.md` |
| 全局变量文档 | `docs/code_flow_analysis/05_global_variables_data_flow.md` |
| 模块关系文档 | `docs/code_flow_analysis/06_module_relationship.md` |
| 重构建议文档 | `docs/code_flow_analysis/09_refactor_reference.md` |
| 启动与运行源码 | `103 + 309/Project/Source/main.c`, `AppInit.c`, `Runtime.c` |
| 构建配置源码 | `103 + 309/Project/Source/conf/Project_Config.h`, `Project_BuildGuard.h`, `conf.h`, `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx` |
| 通信源码 | `103 + 309/Project/Source/Sci_Upper.c`, `Sci_Upper.h`, `Can_HDX.c` |
| 低功耗源码 | `103 + 309/Project/Source/rtc_sleep.c`, `rtc_sleep_port.c`, `SleepDeal.c`, `RTC.c` |
| 采样/SOC源码 | `103 + 309/Project/Source/DataDeal.c`, `SOC.c`, `SocEnhance.c` |

## 核心判断

当前不建议直接进入源码重构。第一阶段应先冻结外部行为，原因如下：

| 观察 | 判断 |
| --- | --- |
| `main()` 和 `Runtime_RunOnce()` 主骨架清楚 | 主调度层可以保留，不是复杂度来源 |
| `App_AFEGet()` 200ms 链路同时更新采样、保护、MOS、SOC | 调用顺序是安全边界，不能先改 |
| `g_stCellInfoReport` 同时服务协议、SOC、日志、LED、低功耗、CAN | 它是公共数据契约，不只是内部变量 |
| `OtherElement` 通过 SCI/CAN 写入后会触发 AFE/SOC/容量/采样电阻副作用 | 需要先确认每个 offset 的语义 |
| `Can_IsBusy()` 看似查询，实际会更新 `sys_time.last_ext_comm_cnt_can` | 低功耗判断依赖这个副作用 |
| 低功耗 blocker 横跨 SCI/CAN/LED/Flash/故障/老化/电流 | 必须先做状态机和 blocker 表 |
| `new_todo_logi()` 名称不表达职责，但影响 MOS、认证熔断、AFE 异常关断 | 适合作为后续第一批等价拆分对象 |

## 需求确认总表

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| REQ-BUILD-001 | 量产构建必须明确关闭 debug/watch/monitor/IRQ debug | `Project_BuildGuard.h`, `Project_Config.h`, `CommomSH367309_16series_103RCT6_C.uvprojx` | `FD_Debug` 目标显式定义 `PROJECT_CFG_BUILD_PROFILE=1` 和 debug 宏；`FD_Release` 未在 uvprojx 中显式定义 profile，源码默认 `PROJECT_CFG_BUILD_PROFILE=0`；但 `Project_Config.h` 当前 debug monitor/IRQ debug 默认是 1 | 可能导致 Release 构建报错，或调试宏误入量产 | CONFLICT | `FD_Release` 当前是否应完全关闭 debug/watch/monitor/IRQ debug？ | 量产 profile 保持 0，并把所有 debug 宏确认为 0；Debug profile 才启用 | 待确认 |
| REQ-BUILD-002 | App 烧录地址必须保持 `0x08004800`，不能覆盖 IAP | `CommomSH367309_16series_103RCT6_C.uvprojx`, AGENTS 安全规则 | Keil 工程存在 `OCR_RVCT4 StartAddress=0x8004800`，但 `LDads/TextAddressRange` 显示 `0x08000000`，是否最终使用 scatter 需结合 Keil target 验证 | 错烧地址会覆盖 Bootloader/IAP | MUST_KEEP | 当前 Release 是否固定使用 `FD_Release.sct` 作为最终链接脚本？ | 保持 App 起始地址 `0x08004800`，烧录继续使用安全脚本 | 待确认 |
| REQ-WDOG-001 | `PROJECT_CFG_WDOG_ENABLE=1` 时量产是否必须启用 IWDG | `Project_Config.h`, `AppInit.c`, `System_Init.c` | 宏当前为 1，但 `AppInit_InitDevice()` 中 `Init_IWDG()` 被注释；`Feed_IWatchDog` 仍在部分路径使用 | 量产可能没有硬件看门狗保护；也可能是低功耗调试期间故意关闭 | UNKNOWN | `Init_IWDG()` 被注释是临时调试还是当前产品设计？ | 若量产要求看门狗，应恢复启用并补低功耗喂狗验证；若故意关闭，应关闭宏并写明原因 | 待确认 |
| REQ-LPDBG-001 | 真实低功耗测试必须关闭 DBGMCU 低功耗调试位 | `conf.h`, `System_Init.c` | `__EnableLowPowerDebug__` 当前定义；`EnableLowPowerDebug()` 会设置 DBGMCU sleep/stop/standby/IWDG/WWDG debug mask | 功耗测试结果会偏高或行为不等同量产 | CHANGE_NEEDED | 当前目标是调试方便还是测真实低功耗？ | 调试构建允许开启；量产/功耗测试构建必须关闭 | 待确认 |
| REQ-AFE-001 | `App_AFEGet()` 内部调用顺序必须保持稳定 | `DataDeal.c`, `docs/code_flow_analysis/03_main_loop_flow.md` | 200ms 到期后依次执行 AFE monitor、电芯电压、max/min、温度、电流、AFE current seq、SH367309、`new_todo_logi()`、SOC | 改顺序可能影响保护、MOS、SOC、电流方向和低功耗判定 | MUST_KEEP | 是否允许后续只做等价命名/局部拆分，不调整调用顺序？ | 第一轮重构禁止改变顺序 | 待确认 |
| REQ-SOC-001 | `SOC_IntEnhance_Ctrl()` 顶层顺序必须保持稳定 | `SocEnhance.c` | 当前顺序为 command、direction、integrate、sag hold、low-tail/full-empty、rest、save、publish | SOC 显示、容量、RTC 休眠补偿可能发生行为漂移 | MUST_KEEP | 后续 SOC 优化是否以回放测试通过为前置条件？ | 保持顶层顺序，先加测试/文档，不先重写算法 | 待确认 |
| REQ-DATA-001 | `g_stCellInfoReport` 是协议与运行状态公共契约 | `Sci_Upper.h`, `Sci_Upper.c`, `DataDeal.c`, `CanFeidaoFrames.c`, `SocEnhance.c`, `LogRecord.c`, `LedBar.c`, `rtc_sleep_port.c` | 电芯、电压、电流、温度、SOC、故障位被 Modbus/CAN/SOC/低功耗/LED/日志共享 | 字段单位或布局变化会破坏协议和业务逻辑 | MUST_KEEP | 是否把 `g_stCellInfoReport` 字段布局视为客户可见协议，不允许直接改？ | 保持布局和单位，先补字段级表 | 待确认 |
| REQ-DATA-002 | `DISP_VBAT_AND_TEMP_` 写 `u16VCell[29/30]` 的协议含义需要确认 | `conf.h`, `DataDeal.c` | 宏当前定义；`new_todo_logi()` 把 Type-C 等效电流写到 `u16VCell[29]`，ADC 总压写到 `u16VCell[30]` | `u16VCell[]` 名义上是电芯电压数组，复用高索引可能误导上位机和后续重构 | UNKNOWN | 这两个字段是否仍是客户可见协议需求？ | 如果必须保留，文档标注为兼容字段；如果废弃，需先确认上位机不再读取 | 待确认 |
| REQ-SCI-001 | `PROJECT_CFG_HOST_WRITE_ENABLE=1` 时 SCI 写寄存器会修改 Flash 与运行状态 | `Project_Config.h`, `Sci_Upper.c` | Host write 当前启用；`Sci_Deal_WrRegs_0x10()` 会处理 AFE、保护、OtherElement、校准、RTC、IAP、SOC/reset/function 等写入 | 协议写入副作用分散，重构时容易漏掉保存或恢复 | MUST_KEEP | 量产是否允许上位机继续写参数？ | 保持启用，但先建立寄存器副作用表 | 待确认 |
| REQ-SCI-002 | `OtherElement` 写入副作用必须保持 | `Sci_Upper.c`, `EEPROM.c`, `DataDeal.h` | offset `0..7`、`8..15`、`28..31` 可能置 `AFE_PARAM_WRITE_Flag`；offset `24..27` 触发 `InitData_SOC()` 和 `SOC_RequestCapacityReset()`；offset `28..31` 更新 `SeriesNum/g_u32CS_Res_AFE` | 参数写入后不立即生效会影响 AFE、SOC、采样电阻、电池串数 | MUST_KEEP | 这些 offset 分组是否全部仍有效？ | 第一轮只文档化，不改副作用 | 待确认 |
| REQ-SCI-003 | 保护参数写入后触发 SOC 重新初始化的行为需要确认 | `Sci_Upper.c` | `Sci_ApplyProtectSideEffects()` 在部分保护参数范围重叠时调用 `InitData_SOC()` | 保护参数和 SOC 配置耦合不直观，但可能依赖 full/empty 电压 | KEEP_BUT_REFACTOR | 写保护参数后重载 SOC 是否是当前需求？ | 先保留；后续明确触发原因并命名 | 待确认 |
| REQ-SCI-004 | `0xD000` 只读窗口是主实时数据窗口 | `Sci_Upper.h`, `Sci_Upper.c` | 注释说明 `0xD000` 主要放 `g_stCellInfoReport`，当前 `RS485_RO_BASE_WORDS=98`，另保留 16 word SOC_TEST padding，合计 `RS485_RO_TOTAL_WORDS=114` | 改长度/顺序会破坏上位机实时监控 | MUST_KEEP | 上位机是否仍按旧顺序读取 `0xD000`？ | 保持地址、长度、顺序，先生成完整字段映射 | 待确认 |
| REQ-SCI-005 | `0xC002` 序列号/版本读取窗口必须保持 | `Sci_Upper.h`, AGENTS 协作规则 | 源码定义 `RS485_ADDR_SN_READ=0xC002`，AGENTS 说明上位机实时监控底部读取 `0xC002` 的 48 个寄存器显示 SN/HW/SW | 改地址或长度会破坏上位机版本显示 | MUST_KEEP | 是否确认 `0xC002` 48 寄存器是当前上位机固定协议？ | 保持不改 | 待确认 |
| REQ-CAN-001 | CAN APP 命令 ID 和 ACK ID 必须保持 | `Can_HDX.c` | 命令帧 StdId 低位为 `0x60`，ACK 低位为 `0x61`，最终 StdId 加 `CAN_ADRESS_STD_ID << 7` | 上位机/CAN 工具不兼容 | MUST_KEEP | 当前 CAN 工具是否固定使用 `0x60/0x61`？ | 保持不改 | 待确认 |
| REQ-CAN-002 | CAN APP 读写寄存器通过 SCI Host Bridge | `Can_HDX.c`, `Sci_Upper.c` | `READ_REG/READ_BLOCK/WRITE_COMMIT` 调用 `Sci_HostReadWords()` 或 `Sci_HostWriteWords()`，共享 Modbus 寄存器权限与副作用 | CAN 写参数和串口写参数行为必须一致 | MUST_KEEP | 后续是否把 CAN APP 视作 Modbus 寄存器桥的一个入口？ | 保持桥接，先补 CAN 命令表 | 待确认 |
| REQ-CAN-003 | CAN 进入 IAP 命令会延迟置 `u8FlashUpdateFlag` | `Can_HDX.c`, `Sci_Upper.c` | `ENTER_IAP` 校验 `0xC3/0x3C/CAN_ADRESS_STD_ID`，`AppUpgrade_RequestIap()` 成功后返回 `0x08/0x48`，20 个 10ms tick 后置 `u8FlashUpdateFlag=1` | IAP 时序和地址提示变化会影响升级流程 | MUST_KEEP | 这个 200ms 延迟和 ACK 值是否固定协议？ | 保持不改，后续只补文档 | 待确认 |
| REQ-CAN-004 | `Can_IsBusy()` 的副作用必须显式保留或重命名 | `Can_HDX.c`, `rtc_sleep.c` | `LP_GetBlockReason()` 调用 `Can_IsBusy()`；若 CAN 接收计数变化，它会更新 `sys_time.last_ext_comm_cnt_can` 并返回 busy | 如果当作纯查询复用，会改变低功耗进入条件 | KEEP_BUT_REFACTOR | 是否允许后续改名为表达“消费通信活动”的函数？ | 先保留行为，后续只做命名和调用点收敛 | 待确认 |
| REQ-LP-001 | 低功耗 blocker 集合必须保持 | `rtc_sleep.c`, `rtc_sleep_port.c` | blocker 包括充电电流、放电电流、SCI/CAN busy、MCU wake key、外部通信计数、老化、Flash busy、升级、故障、LED active | 漏掉 blocker 会导致误睡眠；多余 blocker 会导致进不了低功耗 | MUST_KEEP | 哪些 blocker 是客户体验必须保留，哪些只是调试期保守条件？ | 先不删 blocker，画状态机和计数来源表 | 待确认 |
| REQ-LP-002 | `HICCUP_MODE` RTC STOP 循环会持续休眠并做 SOC 休眠补偿 | `rtc_sleep.c`, `rtc_sleep_port.c` | RTC 唤醒且无异常时继续下一轮 STOP；每轮累加 sleep 秒数，调用 `SOC_ApplyRtcRelaxationCompensation()` | 这个循环会阻塞主循环，但可能是目标低功耗策略 | KEEP_BUT_REFACTOR | 当前产品是否要求 HICCUP 连续睡眠，还是只睡一次后回主循环？ | 先保留行为，后续用实测确认用户体验和功耗 | 待确认 |
| REQ-LP-003 | `NORMAL_MODE/DEEP_MODE` 通过 reset-sleep 链路进入睡眠 | `rtc_sleep.c`, `rtc_sleep_port.c`, `SleepDeal.c` | `low_power_log_and_commit_sleep()` 记录 sleep log 后调用 `SleepDeal_Continue(sleep_mode)` | 与 HICCUP STOP 路径不同，不能混为一类重构 | MUST_KEEP | 两条低功耗路径是否都仍用于当前硬件？ | 分开建模，不合并路径 | 待确认 |
| REQ-MOS-001 | `new_todo_logi()` 的 MOS 过温动作必须确认 | `DataDeal.c` | MOS 温度 >= 95 C 关闭 `ctlc` 并记录 `MosOTp_Third`；降到 <= 75 C 后打开 `ctlc` | 阈值或动作错误会影响保护和恢复 | MUST_KEEP | MOS 过温只关 `ctlc` 是否符合当前硬件设计？ | 先等价拆名，不改阈值/动作 | 待确认 |
| REQ-FUSE-001 | `_UL_RENZHENG_ENABLE_` 认证熔断逻辑必须确认 | `conf.h`, `DataDeal.c` | 宏当前定义；AFE 通信异常、温度、电芯过压、ADC 总压过压、充电状态共同影响 `GPIO_RF_EN` | 触发条件高风险，误改可能导致保险丝误动作或失效 | MUST_KEEP | 该认证熔断逻辑是否仍是量产需求？阈值是否客户确认？ | 未确认前禁止改条件 | 待确认 |
| REQ-AGING-001 | 工厂老化状态影响 CAN 命令和低功耗 | `FactoryAging.c`, `Can_HDX.c`, `rtc_sleep.c` | CAN APP 支持 aging start/stop/reset/set hours；`LP_GetBlockReason()` 在 `NO_SLEEP` 且老化 active 时阻塞低功耗 | 老化期间误睡会影响测试；命令变化影响上位机 | MUST_KEEP | 当前老化命令和低功耗阻塞是否都保留？ | 保持不改 | 待确认 |
| REQ-LOG-001 | 睡眠前日志记录必须保留 | `rtc_sleep_port.c`, `LogRecord.c` | reset-sleep 前调用 `LogRecord_RequestSleep()` 和 `LogEvent_Record(BMS_SLEEP)` | 删除会丢失睡眠事件记录 | KEEP_BUT_REFACTOR | 睡眠日志是否是现场问题追踪必需？ | 保持，后续只优化调用命名 | 待确认 |

## 协议与状态冻结清单

### 构建宏冻结清单

| 宏/目标 | 当前源码状态 | 影响 | 冻结建议 |
| --- | --- | --- | --- |
| `FD_Release` | uvprojx 中 `Define` 只有 `STM32F10X_MD,USE_STDPERIPH_DRIVER` | 未显式覆盖 `PROJECT_CFG_BUILD_PROFILE` | 必须实测 Keil Release 预处理/构建结果 |
| `FD_Debug` | uvprojx 中定义 `PROJECT_CFG_BUILD_PROFILE=1`、debug watch/monitor/IRQ debug | Debug 可观测性打开 | Debug 与 Release 明确隔离 |
| `PROJECT_CFG_BUILD_PROFILE` | `Project_BuildGuard.h` 默认 0 | profile 0 触发 release debug 禁用检查 | Release 保持 0，Debug 使用 1 |
| `PROJECT_CFG_DEBUG_WATCH_ENABLE` | `Project_BuildGuard.h` 默认 1；Debug 目标显式 1 | 启用 `DEBUG_WATCH_ENABLED` | Release 应为 0 |
| `PROJECT_CFG_DEBUG_MONITOR_ENABLE` | `Project_Config.h` 默认 1；Debug 目标显式 1 | 编译 `SystemDebug.c`/快照 | Release 应为 0 |
| `PROJECT_CFG_IRQ_DEBUG_ENABLE` | `Project_Config.h` 默认 1；`Project_BuildGuard.h` 默认 0；Debug 目标显式 1 | 编译中断计数/事件 | Release 应为 0 |
| `PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE` | `Project_Config.h` 默认 1；Debug 目标显式 0 | 事件环依赖 IRQ debug | Release 应为 0；Debug 是否开启事件环需确认 |
| `PROJECT_CFG_WDOG_ENABLE` | 当前 1 | 编译 `Init_IWDG()` 内部逻辑 | 需与 `AppInit.c` 中注释状态对齐 |
| `__EnableLowPowerDebug__` | `conf.h` 当前直接定义 | 设置 DBGMCU 低功耗调试位 | 真实功耗测试/量产必须关闭 |

### SCI/Modbus 冻结清单

| 区域/地址 | 当前含义 | 写入入口 | 关键副作用 | 冻结建议 |
| --- | --- | --- | --- | --- |
| `0xD000` | 主要映射 `g_stCellInfoReport` | 只读 | 上位机实时监控/CAN bridge 可读 | 地址、顺序、长度不改 |
| `0xD100` | RTC 开始的一组只读数据 | 只读 | RTC/尾部状态信息 | 先补完整字段表 |
| `0xD200` | fault snapshot | 只读 | 故障原因与反码 | 保持 |
| `0xC000` | LCD 只读窗口 | 只读 | 未完全展开 | 需补字段表 |
| `0xC001` | RTC/FA 只读窗口 | 只读 | 老化/RTC 状态可能读取 | 需补字段表 |
| `0xC002` | SN/HW/SW 读取窗口 | 只读 | 上位机固定读取 48 寄存器 | 必须保持 |
| `0xC008` | event record | 只读 | 日志/事件读取 | 保持 |
| `0x1000` 起 | reset/set once SOC/function 类命令 | `0x10`/`0x06` 写 | 可能重置参数、事件、SOC、功能开关 | 后续逐项展开 |
| `0x2000` 起 | 校准 K/B | `Sci_WrRegs_0x10_CalibCoef()` | 保存校准系数 | 保持写入范围校验 |
| `0x2100` 起 | 保护参数 | `Sci_WrRegs_0x10_Protect()` | 保存 Flash，部分触发 `InitData_SOC()` | 保持副作用 |
| `0x2200` 起 | SOC 表/RTC 等 | `Sci_WrRegs_0x10_SocTable()` 当前负应答；RTC 函数空实现 | 当前不是完整可写 | 需确认是否废弃或预留 |
| `0x2300` 起 | `OtherElement` | `Sci_WrRegs_0x10_OtherElement()` | AFE/SOC/SeriesNum/采样电阻副作用 | 必须先确认 offset 表 |
| `0xFFFD` | Flash/IAP connect | `Sci_WrRegs_0x10_FlashConnect()` | `AppUpgrade_RequestIap()` | IAP 路径不改 |

### `OtherElement` 写入副作用冻结表

| offset 范围 | 对应区域 | 当前副作用 | 风险 |
| --- | --- | --- | --- |
| `0..7` | balance/open time 类参数 | `AFE_PARAM_WRITE_Flag=1`，仅 `AFE_TYPE==sh36xx` 分支实际置位 | AFE 参数可能不写回 |
| `8..15` | 采样/短路/冷却等系统参数 | `AFE_PARAM_WRITE_Flag=1` | 保护阈值/AFE 参数可能不生效 |
| `24..27` | SOC 容量/循环/保留 | `InitData_SOC()`，`SOC_RequestCapacityReset()` | 改写容量后 SOC 未重载或错误清容量 |
| `28..31` | 串数/采样电阻/分子 | `AFE_PARAM_WRITE_Flag=1`，更新 `SeriesNum` 和 `g_u32CS_Res_AFE` | 电流换算、串数、电压阈值错误 |

### CAN APP 命令冻结清单

| 命令 | 值 | 当前行为 | 副作用/返回 |
| --- | --- | --- | --- |
| `GET_STATUS` | `0x01` | 读取 SOC/SOH 百分比 | ACK `value0=SOC`, `value1=SOH` |
| `ENTER_IAP` | `0x02` | 校验 `0xC3/0x3C/CAN_ADRESS_STD_ID`，请求 IAP | ACK `0x08/0x48`，延迟 20 个 10ms tick 后 `u8FlashUpdateFlag=1` |
| `READ_REG` | `0x03` | 通过 `Sci_HostReadWords()` 读 1 word | 共享 SCI 读权限 |
| `WRITE_PREP` | `0x04` | 缓存写地址和高字节 | 需要后续 commit |
| `WRITE_COMMIT` | `0x05` | 校验地址后组合 16bit 值，调用 `Sci_HostWriteWords()` | 共享 SCI 写副作用 |
| `READ_BLOCK` | `0x06` | 最多读 120 words 到 `s_app.read_block_words` | ACK 后按 1 tick 间隔发送 `0x86` 数据帧 |
| `AGING_START` | `0x07` | 校验 aging guard/action/id，启动老化 | 返回老化状态和剩余小时 |
| `AGING_STOP` | `0x08` | 校验后停止老化 | 返回老化状态和剩余小时 |
| `AGING_RESET_TIME` | `0x09` | 校验后清老化时间 | 返回老化状态和剩余小时 |
| `AGING_SET_HOURS` | `0x0A` | 校验小时范围和 id 后设置老化时长 | 返回老化状态和剩余小时 |
| `READ_BLOCK_DATA` | `0x86` | 不是主机命令，是固件分包返回命令号 | Data[3]=seq, Data[4..5]=word |

### 低功耗 blocker 冻结清单

| blocker | 当前来源 | 条件 | 影响 |
| --- | --- | --- | --- |
| `LP_BLOCK_CHARGE` | `g_stCellInfoReport.u16Ichg` | `>10` | 阻止进入 HICCUP/NORMAL/DEEP 选择 |
| `LP_BLOCK_DISCHARGE` | `g_stCellInfoReport.u16IDischg` | `>10` | 阻止低功耗 |
| `LP_BLOCK_COMM` | `Sci_IsAnyPortBusy()` 或 `Can_IsBusy()` | 任一 busy | 阻止低功耗；`Can_IsBusy()` 会消费 CAN 活动计数 |
| `LP_BLOCK_KEY` | `GPIO_MCU_WK` | wake active | 阻止低功耗 |
| `LP_BLOCK_EXT_COMM` | `SleepDeal_GetExternalCommCounter()` | 与 `g_stLowPowerRtcStatus.comm` 不同 | 阻止低功耗并更新计数 |
| `LP_BLOCK_AGING` | `FactoryAging_IsActive()` | `mode==NO_SLEEP` 且老化 active | 老化期间阻止睡眠 |
| `LP_BLOCK_FLASH_BUSY` | `StorageFlash_IsBusy()` 或 `u8FlashUpdateE2PROM` | busy | 防止写 Flash 时睡眠 |
| `LP_BLOCK_UPGRADE` | `u8FlashUpdateFlag` | 非 0 | IAP/升级期间阻止睡眠 |
| `LP_BLOCK_FAULT` | `g_stCellInfoReport.unMdlFault_Third.all` | 非 0 | 故障期间阻止睡眠 |
| `LP_BLOCK_LED_ACTIVE` | `LedBar_IsActiveForLowPower()` | LED 显示活跃 | 防止显示期间立刻睡眠 |

### `g_stCellInfoReport` 公共字段冻结清单

| 字段 | 单位/含义 | 主要写入 | 主要读取 | 冻结建议 |
| --- | --- | --- | --- | --- |
| `u16VCell[0..31]` | mV；但高索引存在兼容复用 | `DataLoad_CellVolt()`, `new_todo_logi()` | SCI 只读窗口、CAN、SOC/调试 | 布局不改；确认 `[29/30]` 兼容含义 |
| `u16VCellMax/Min/Delta` | mV | `DataLoad_CellVoltMaxMinFind()` | SOC、低功耗、CAN、日志/保护 | 单位不改 |
| `u16VCellTotle` | `V*100`，发送时常乘 10 得 mV | `DataDeal.c` | CAN、SOC、上位机 | 单位必须写入协议表 |
| `u16Temperature[]` | `(C + 40) * 10` | `DataLoad_Temperature()` | 保护/MOS/CAN/上位机 | 单位不改 |
| `u16Ichg/u16IDischg` | `A*10` | `DataLoad_Current()` | SOC、低功耗、CAN、保护 | 单位不改；注意 `rtc_sleep_port.c` 函数名写 `Ma` 但返回 A*10 |
| `SocElement` | SOC/SOH/容量/循环 | `soc_publish()` | 上位机、CAN、LED、调试 | 字段语义不改 |
| `unMdlFault_*` | 故障位 | AFE/保护/故障函数 | 上位机、CAN、日志、低功耗、LED | bit 位不改 |
| `u16BalanceFlag1/2` | 均衡标志 | AFE/数据处理 | 上位机/调试 | 未确认完整写入链 |

### `new_todo_logi()` 行为冻结清单

| 行为块 | 当前条件/动作 | 后续处理建议 |
| --- | --- | --- |
| 充电器检测和按键逻辑 | 调用 `charger_detect_and_keyLogi_200ms()` | 后续只改函数名/归属，不改调用时机 |
| 兼容显示字段 | `DISP_VBAT_AND_TEMP_` 下写 `u16VCell[29/30]` | 先确认协议含义 |
| MOS 过温 | MOS 温度 >= 95 C 关闭 `ctlc`；<= 75 C 恢复 | 可等价拆为 `DataDeal_ProcessMosOverTemp()` |
| AFE 通信异常处理 | `System_ErrFlag.u8ErrFlag_Com_AFE1==1` 时关闭 `ctlc`，特定高压/高温持续后拉 `GPIO_RF_EN` | 高风险，不改条件 |
| UL 认证熔断 | `_UL_RENZHENG_ENABLE_` 下温度/过压/充电持续触发 `GPIO_RF_EN` | 必须人工确认阈值 |
| 故障记录 | 记录 `MosOTp_Third`, `CellChgOTp_Third`, `CellDsgOTp_Third`, `CellOvp_Third`, `BatOvp_Third` | 保持 |

## 建议执行顺序

| 阶段 | 目标 | 允许动作 | 禁止动作 | 验证方式 |
| --- | --- | --- | --- | --- |
| Phase 0 | 需求确认与行为冻结 | 继续补表、确认宏、确认协议字段 | 改源码 | 文档审阅、Keil 预处理/构建确认 |
| Phase 1 | 低风险等价整理 | 重命名、拆分 `new_todo_logi()`，补注释/单位文档 | 改条件、改调用顺序、改协议 | `git diff --check`、Keil build、必要 host/static 检查 |
| Phase 2 | 协议桥收口 | 建 SCI/CAN 寄存器表，减少重复分支 | 改地址、长度、ACK、错误码 | 上位机读写回归、CAN APP 回归 |
| Phase 3 | 低功耗净删减 | 基于 blocker 表简化状态变量 | 未实测就删 blocker 或合并 STOP/reset-sleep | 串口/CAN/电流/RTC 唤醒实测 |
| Phase 4 | SOC 局部优化 | 基于回放测试修正显示/保存/RTC 补偿 | 重写状态机顶层顺序 | SOC host/replay 测试、板端读寄存器 |

## 必须人工确认的问题

| ID | 问题 | 为什么必须确认 |
| --- | --- | --- |
| Q1 | `FD_Release` 是否必须能在当前源码状态下无 debug 宏构建通过？ | 关系到量产隔离和 BuildGuard 改法 |
| Q2 | `Init_IWDG()` 注释是否是临时调试？ | 关系到看门狗安全边界 |
| Q3 | `__EnableLowPowerDebug__` 是否只允许 Debug 构建使用？ | 关系到真实低功耗电流 |
| Q4 | `u16VCell[29]` 和 `u16VCell[30]` 是否仍是上位机读取字段？ | 关系到 `g_stCellInfoReport` 布局能否清理 |
| Q5 | `_UL_RENZHENG_ENABLE_` 熔断逻辑是否为当前量产需求？ | 关系到不可逆硬件动作 |
| Q6 | CAN APP `0x60/0x61`、`0x01..0x0A` 命令是否保持兼容？ | 关系到上位机/CAN 工具 |
| Q7 | 低功耗 blocker 是否全部保留？ | 关系到误睡眠和进不了低功耗之间的取舍 |
| Q8 | HICCUP 连续 STOP 循环是否是目标体验？ | 关系到主循环阻塞与通信响应 |

## 未确认项

| 项 | 当前状态 | 下一步 |
| --- | --- | --- |
| Keil `FD_Release` 实际预处理结果 | 未构建验证 | 用 Keil/脚本验证宏最终值 |
| `FD_Release.sct` 是否为当前目标最终链接脚本 | 仅从规则和工程片段推断 | 读取完整 target 配置或构建 map |
| `0xD000` 完整字段顺序 | 已知主窗口和长度，未逐 word 展开 | 继续从 `Sci_Upper.c` 读出填表 |
| `0xC002` 48 寄存器具体字段 | AGENTS 和宏已确认地址，未逐 word 展开 | 继续读取 `Sci_Upper.c` SN 分支 |
| `SleepDeal_Continue()` reset-sleep 细节 | 本文未展开 | 下一轮分析 `SleepDeal.c` |
| `FactoryAging` 状态字段 | 本文只确认低功耗/CAN 入口 | 下一轮补老化协议表 |
| `CanFeidaoFrames` 周期帧字段 | 本文只确认 APP 命令 | 下一轮补 1000ms/5000ms 周期帧表 |
