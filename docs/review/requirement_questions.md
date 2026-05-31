# 103-309 BMS 需求合理性判断与确认问题表

> 本表以当前源码为第一依据。分类含义：`MUST_KEEP` 必须保留；`KEEP_BUT_REFACTOR` 保留需求但可改实现；`CHANGE_NEEDED` 需求或实现需要修改；`MAYBE_UNUSED` 可能不用；`MISUNDERSTOOD` 可能理解错；`CONFLICT` 与其他需求冲突；`UNKNOWN` 必须用户确认；`REMOVE_CANDIDATE` 可考虑删除但不能直接删。

## 0. 2026-05-31 七 agent 综合确认表

状态：部分验证。本节来自架构、安全、通信、低功耗、存储、SOC、可读性 7 个只读 agent 结论，并回到当前源码做交叉核对。本节只用于需求确认；在用户逐条确认前，不代表允许修改源码。

参考源码：

- `103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.sct`
- `103 + 309/Project/Source/Flash.h`
- `103 + 309/Project/Source/Flash.c`
- `103 + 309/Project/Source/SH367309_DataDeal.c`
- `103 + 309/Project/Source/I2C_AFE1.c`
- `103 + 309/Project/Source/SH367309_Func.c`
- `103 + 309/Project/Source/DataDeal.c`
- `103 + 309/Project/Source/SleepDeal.c`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/Sci_Upper.c`
- `103 + 309/Project/Source/SocEnhance.c`
- `103 + 309/Project/Source/conf/Project_Config.h`

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| Q-7A-FLASH-001 | App 链接区必须与后 16KB 持久化存储区硬隔离 | `CommomSH367309_16series_103RCT6_C.sct:5-13` 将 `LR_IROM1/ER_IROM1` 设为 `0x08004800 + 0x00020000`；`Flash.h:7-29` 从 `0x0801C000` 开始定义 AFE/RW/LOG/SOC 存储页 | 当前 scatter 允许 App 链接区覆盖 `0x0801C000+` 存储页；安全脚本只约束烧录起始地址，不等价于链接区结束地址检查 | P0：App 体积增长后可能覆盖参数/SOC/日志页，或发布时误判安全 | CHANGE_NEEDED | 量产 MCU 实际 Flash 容量是多少？App 最大结束地址是否必须小于 `0x0801C000`？后 16KB 是否固定保留存储？ | C. 修改链接/门禁边界；E. 暂不改但禁止发布；F. 补充 BOM/Keil map 证据 | |
| Q-7A-IAP-001 | App->IAP SRAM mailbox 必须被链接脚本保留 | `Flash.c:12` 使用 `APP_UPGRADE_MAILBOX_ADDR 0x20004FE0`；`CommomSH367309_16series_103RCT6_C.sct:12-13` 将 `RW_IRAM1` 设为 `0x20000000 + 0x00005000` | mailbox 地址落在当前 RW_IRAM1 范围尾部，存在被链接器分配 RW/ZI 的风险 | P0：IAP 请求可能被运行时变量覆盖，升级进入 IAP 不稳定 | CHANGE_NEEDED | IAP 固件是否固定读取 `0x20004FE0`？是否允许调整 mailbox，还是只能在 App scatter 中保留该地址？ | B. 保留该地址并修 scatter/map 检查；C. 修改 mailbox 协议；F. 补充 IAP 固件证据 | |
| Q-7A-UPGRADE-001 | 升级参数清除策略必须按量产/特殊升级包隔离 | `Project_Config.h:411-447` 默认启用 policy `0x0005`，并开启 AFE、保护、SOC 表、SOC 配置、SOC snapshot、事件记录、老化时间 reset；`EEPROM.c:255-287` 中 SOC 表 reset 还受 `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE` 门控，老化 reset 会调用 `FactoryAging_ResetTimeByHost()` | 当前量产配置下升级后可能清空多类现场数据；SOC table 宏开启但当前 runtime table 关闭时不实际清表；老化 reset 会进入 host running 路径；标志在动作后写入，掉电时可能部分执行后下次重跑 | P0/P1：现场参数、SOC、事件记录、老化时间可能被非预期清空或重启老化流程 | UNKNOWN | 当前 `0x0005` 是否就是本批量产升级要求？哪些数据允许清，哪些必须保留？老化 reset 是否允许启动/恢复 running-from-host？ | B. 保留但按版本/特殊包文档化；C. 改为默认不清；F. 补充本批升级策略 | |
| Q-7A-AFE-001 | 主机写 AFE 参数前必须做与上电加载一致的范围校验 | `SH367309_DataDeal.c:249-283` 写 `0x2400+` 时直接写 `AFE_Parameters_RS485_Struction` 的 `curValue` 并保存；参数结构有 min/max 但写入口未校验；`Refresh_Parameters()` 会换算后写 MTP | 保护参数可由上位机写入后触发 AFE MTP/ROM 更新，非法值可能进入硬件参数 | P0：保护阈值错误、NTC 表下标越界、MOS/AFE 行为异常 | CHANGE_NEEDED | 量产是否允许上位机/CAN 写 AFE 参数？非法值应返回协议错误还是兼容旧上位机静默限制？ | C. 写前严格校验并拒绝；B. 加权限层后保留；F. 补充工装流程 | |
| Q-7A-AFE-002 | AFE 通信失败后必须有明确失效安全策略 | `I2C_AFE1.c:617-645` `MTPRead()` 失败只置 `ERROR_AFE1`；`SH367309_Func.c:307-335` 读失败分支没有明确安全动作；`DataDeal.c` 仍会继续使用全局状态 | 当前 AFE 读失败可能只上报错误，旧电压/温度/MOS 状态仍可能继续参与 SOC、显示、低功耗和保护辅助判断 | P0：AFE 状态未知时仍保持输出或进入低功耗，安全边界不清 | UNKNOWN | AFE 连续通信失败时应断充放电 MOS、关闭 CTLC、保持现状、复位 MCU，还是只上报？连续失败阈值是多少？ | C. 定义 fail-safe 状态；B. 保留但补可观测/重试；F. 补充客户安全策略 | |
| Q-7A-AFE-003 | AFE watchdog 是否必须启用 | `SH367309_Func.c:136-148` 注释说明 `ENWDT` 未打开，只开启 CADC 和 MOS 控制位；MCU IWDG 不能替代 AFE 侧独立保护 | 当前依赖 SH367309 自身硬件保护和 MCU 轮询，没有启用 AFE watchdog 溢出关 MOS/均衡机制 | P0/P1：MCU 卡死或 I2C 异常时 AFE 侧独立失效保护不足 | UNKNOWN | 309 当前硬件和产品安全策略是否要求启用 AFE watchdog？若不启用，风险接受依据是什么？ | A. 明确保留关闭并文档化；C. 评估启用；F. 补充 AFE datasheet/实测 | |
| Q-7A-MOS-001 | `new_todo_logi()` 中 MOS/CTLC/UL 逻辑是否允许进入量产 | `DataDeal.c:1086-1145` 每 200ms 调用 `charger_detect_and_keyLogi_200ms()`，并按硬编码温度阈值 `95/75C` 调用 `close_ctlc()/open_ctlc()`；`_UL_RENZHENG_ENABLE_` 路径含 TODO 和熔断逻辑 | 生产路径中存在未确认 TODO、硬编码阈值和 CTLC 恢复动作 | P0/P1：MOS/CTLC 恢复条件不完整，认证/保护逻辑可能误动作 | CHANGE_NEEDED | 这些 UL/熔断/CTLC 逻辑是当前客户需求、认证临时代码，还是历史测试残留？恢复 CTLC 前需要哪些 AFE fault/MOS 读回条件？ | B. 保留但补条件和文档；C. 改为显式配置隔离；D. 删除历史路径；F. 补充认证需求 | |
| Q-7A-LP-001 | reset-sleep 合法唤醒源必须与 EXTI 配置一致 | `conf.c:215-266` 配置 UART1 RX、CHG_IN、INT_WK_CMNT、MCU_WK 唤醒；`SleepDeal.c:22-65` `IsSleepWakeupValid()` 只接受充电器或按键；`SleepDeal.c:186-230` 非法则继续 STOP | UART/CMNT/MCU_WK 可触发 STOP 唤醒，但可能不被认定为合法启动唤醒，导致继续进入 STOP | P0/P1：通信/CMNT 唤醒无效，设备看似无法唤醒或错过上位机首帧 | UNKNOWN | reset-sleep 下 CAN/RS485/UART/CMNT/MCU_WK 是否必须唤醒并进入正常运行？哪些源只用于显示预览？ | B. 保留现状并文档化；C. 扩展合法唤醒源；F. 补充硬件唤醒矩阵 | |
| Q-7A-LP-002 | 通信活跃判定不能漏判后进入低功耗 | `SleepDeal.c:4` `RTC_ExtComCnt` 是 `UINT8` 且非 `volatile`；`Sci_Upper.c:1465-1483` USART RX ISR 每字节自增；`rtc_sleep.c` 使用外部通信计数判断 idle | 8-bit 计数回绕或非 volatile 访问可能导致低功耗误判通信空闲 | P1：上位机通信中误入 STOP，造成首帧丢失或交互不稳定 | CHANGE_NEEDED | 是否允许把通信活动标志改成 `volatile uint16_t/uint32_t` 或 last_activity_tick，并把 `Sci_IsAnyPortBusy()` 纳入低功耗判定？ | C. 修改判定；B. 保留但加观测；F. 补充上位机唤醒重发规则 | |
| Q-7A-CAN-001 | CAN 可靠性与低功耗功耗目标需要取舍 | `Can_HDX.c:421-448` TX failed/timeout 后记录并清 mailbox；`Can_HDX.c:1128-1182` RTC wake 服务最多 `150 * 10ms` 等待 CAN 完成 | 周期帧可丢，但 App ACK/IAP/写寄存器/老化控制帧也缺少类型化重试；RTC 唤醒窗口可能阻塞约 1.5s | P1：关键 CAN 帧丢失、低功耗期间主循环/Modbus 延迟、功耗上升 | CONFLICT | 休眠中是否必须保持 1s/5s CAN 可见？关键 ACK/IAP/写寄存器帧是否需要有限重试？ | A. 保留低功耗 CAN 服务；B. 仅关键帧重试；C. 缩短 RTC CAN slice；F. 补充客户协议时序 | |
| Q-7A-PROTO-001 | 空实现或 `#if 0` 的协议写入口不能正 ACK | `Sci_Upper.c:963-995` `Sci_WrRegs_0x10_SocTestMode()` 主体被 `#if 0`；`Sci_Upper.c:1758-1805` 校准写主体被 `#if 0`；`Sci_Upper.c:1876-1883` 铜损/RTC 写为空；`Sci_Upper.c:2015-2067` 校准 reset 主体被 `#if 0` | `Sci_ModbusProcessFrame()` 默认 `AckType=RS485_ACK_POS`；这些 handler 不设置 NEG 时会返回正响应但无动作；SOC table 写入口在关闭时已显式 NEG，需与空 ACK 风险区分 | P1：上位机/CAN App 误判写入成功，测试和量产调参结论错误 | CHANGE_NEEDED | `0x2500`、校准写入、校准 reset、铜损、RTC 写入口是废弃、协议占位，还是后续要恢复？Release 下应返回什么错误码？ | C. 明确返回不支持；B. 测试/工装 profile 恢复；D. 保留读/写占位但文档化；F. 补充上位机兼容要求 | |
| Q-7A-SOC-001 | SOC 校准是否必须在保护/系统故障时阻断 | `Project_Config.h:286-292` 两个 block 宏默认 `0`；`SocEnhance.c:913-940` 满电确认只依赖电压/压差/内部 SOC；`SocEnhance.c:1268-1295` 满电锚点逐步到 100% | 保护或系统故障状态下仍可能进行 OCV/满空校准；满电锚点缺少明确 charger-present 或充电电流 taper 条件 | P1：故障态 SOC 被误校准，满电 100% 锚定可能过早 | UNKNOWN | 量产 SOC 校准是否必须避开 AFE/ADC/温度/保护故障？满电锚点是否需要充电器在线和小电流 taper 条件？ | A. 保留当前体验策略；C. 增加故障阻断和 taper 条件；F. 补充电芯/充电器策略 | |
| Q-7A-STORAGE-001 | Flash/EEPROM 存储失败必须可见且有恢复策略 | `System_Monitor.c:128-148` `ERROR_EEPROM_STORE` 不递增错误标志；`Flash.c:496-660` 多个保存失败路径会调用 `ERROR_EEPROM_STORE`；`DataDeal.c:1019-1024` 睡眠延迟检查存储错误状态；`app_lowpower.c:48-50` 只因 busy/pending 阻塞低功耗 | 存储失败可能不会进入 `ERROR_STATUS_EEPROM_STORE`，低功耗阻塞、日志和上位机观测都不可靠；当前低功耗框架只阻塞正在写或待写，不阻塞持久化失败 latch | P1：参数/SOC/日志写失败不可见，后续仍可能进入睡眠或继续运行 | CHANGE_NEEDED | 存储失败应只上报、阻塞低功耗、限制充放电，还是触发复位/工装维修？错误是否需要可清除？ | B. 上报并阻塞低功耗；C. 加安全动作；F. 补充售后策略 | |

## 1. 必须先确认，否则不能重构

| ID | 模块 | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 风险 | 需要我确认的问题 | 建议选项 | 我的决定 |
|---|---|---|---|---|---|---|---|---|---|
| Q-CRIT-001 | AFE/电流/SOC | 当前量产固件主路径应继续使用真实 AFE CADC 电流，虚拟电流入口是否删除或测试 profile 隔离 | `DataDeal.c:1238-1239` 调用 `DataLoad_Current()`，`test_Autocurrent_cycle()` 为注释状态；`Project_Config.h:17` 为 profile 0 | 200ms 主路径使用真实电流并递增 `g_u32AfeCurrentSampleSeq`；虚拟电流函数仍残留 | KEEP_BUT_REFACTOR | P1：旧文档误导后续判断；残留测试入口若重新接入量产会影响 SOC、CAN 电流、保护状态和老化行为 | 是否删除 `test_Autocurrent_cycle()`，还是迁移到 Factory/Test profile 或 host test？ | B. 保留需求但隔离测试入口；D. 删除残留测试函数；F. 补充现场调试习惯 | |
| Q-CRIT-002 | 均衡 | 当前产品是否需要主动均衡 | 仅找到 `OtherElement.u16Balance_*`、`MTP_BALANCEH/L`、CBC status；未见 `App_CellBalance()` 进入 `Runtime_RunOnce()` | 有均衡参数和状态位，但未确认主动控制 | UNKNOWN | P0/P1：若客户要求均衡，当前实现可能缺功能；若不要求，协议残留增加复杂度 | 均衡是当前客户需求、未来模板需求，还是历史残留？ | A. 保留原需求；B. 保留并重构；D. 删除业务但保留协议占位；F. 补充背景 | |
| Q-CRIT-003 | AFE 参数 | 均衡开启电压是否应使用可写参数 | `SH367309_DataDeal.c:58-59` 注释参数计算，实际硬编码 `4160` | 上位机写 `u16Balance_OpenVoltage` 可能不影响 AFE ROM 对应字段 | MISUNDERSTOOD | P1：上位机参数和硬件行为不一致 | `u16Balance_OpenVoltage` 是否必须驱动 AFE 均衡开压？ | B. 保留需求但修实现；C. 修改为固定 4160 并文档化；F. 补充背景 | |
| Q-CRIT-004 | Flash/IAP | 当前真实硬件 Flash 容量和 App 链接地址是什么 | `Flash.h:4-30` 使用 `0x08004800` App 和 `0x0801C000+` 存储；Keil XML 同时有 `0x08000000` 和 `0x8004800`，`ScatterFile` 为空 | 安全脚本约束 App 烧录 `0x08004800`，但 Keil 工程显示不够单一 | UNKNOWN | P0：地址错会覆盖 IAP 或越界写 Flash | 实际量产芯片是 64KB、128KB 还是 C8 兼容大 Flash？Keil 最终链接地址以哪个文件为准？ | A. 保留现有地址并补 map 验证；C. 修改地址策略；F. 补充硬件/BOM | |
| Q-CRIT-005 | Host 写权限 | 量产固件是否允许上位机写保护/Other/AFE/IAP/SOC | `PROJECT_CFG_HOST_WRITE_ENABLE 1`；`Sci_Upper.c:1818-1984` | 当前量产宏下可写多个关键参数和 IAP 请求 | UNKNOWN | P0/P1：现场误写会影响保护阈值、AFE MTP、IAP 进入 | 量产版本是否需要密码/工装模式/只读模式？ | A. 保留原写权限；B. 保留但加权限层；C. 按新逻辑收紧；F. 补充工厂流程 | |
| Q-CRIT-006 | 低功耗/IWDG | RTC 周期唤醒是否必须兼顾 CAN 周期广播 | `RTC.c:386-390` 限制 10s；`Can_HDX.c:952-979` RTC wake 服务 CAN | STOP 中因 IWDG 最大 10s 频繁醒来，并可短时上 CAN 服务 | CONFLICT | P1：低功耗目标与通信在线需求冲突 | 休眠时是否必须周期发 CAN，还是只在唤醒源触发后通信？ | A. 保留；C. 修改为更省电策略；E. 暂保留后续实测 | |
| Q-CRIT-007 | 低功耗/MOS | 充电器拔除是否应直接进入 deep sleep | `DataDeal.c:63-107` | CHG_IN 状态会影响 MOS，拔除后调用 deep sleep 路径 | UNKNOWN | P1：影响用户体验和安全输出状态 | 当前产品拔 5V 后是关机、待机还是继续运行？ | A. 保留；C. 修改；F. 补充产品行为 | |
| Q-CRIT-008 | 老化模式 | 出厂老化是否仍为当前量产需求 | `PROJECT_CFG_FACTORY_AGING_ENABLE 1`；`FactoryAging.c:587-620` | 默认上电可能自动进入/恢复老化，CAN 可控制 | UNKNOWN | P1：若误保留会影响 MOS、低功耗和出货流程 | 老化是每台板必跑、工厂专用固件，还是旧客户需求？ | A. 保留；B. 保留但工装隔离；D. 删除业务；F. 补充背景 | |
| Q-CRIT-009 | 产品信息 | 默认 SN/硬件/软件版本是否可进入量产 | `DataDeal.h:182-187` 默认 `"T3_27Ah"`, `"D010"`, `"cs-666-8888"`；`Sci_Upper.c:769-773` | `0xC002` 会读出默认/写入后的产品信息 | CHANGE_NEEDED | P1：上位机显示和客户追溯错误 | 量产 SN/HW/SW 来源是烧录、上位机写入、还是固定编译？ | C. 修改为明确量产写入流程；F. 补充生产规则 | |
| Q-CRIT-010 | AFE 抽象 | 长期模板是否必须支持不同 AFE 切换 | `PROJECT_CFG_AFE_TYPE 1`，源码强绑定 `SH367309_*` | 当前并非真正 AFE 接口层 | KEEP_BUT_REFACTOR | P1：直接改会影响保护和低功耗；不改会阻碍模板化 | 第一版模板要抽接口，还是先保留 SH367309 业务稳定？ | B. 保留需求但分阶段抽象；E. 后续确认 | |

## 2. 建议确认，但不影响低风险整理

| ID | 模块 | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 风险 | 需要我确认的问题 | 建议选项 | 我的决定 |
|---|---|---|---|---|---|---|---|---|---|
| Q-MID-001 | 温度 | ENV2/ENV3 是否真实没有传感器 | `DataDeal.c:227-277` | 两个环境温度被强制为 -40 等效值 | UNKNOWN | 上位机显示可能误导 | 是否应显示无效值、隐藏，还是保留 -40？ | A/B/C/F | |
| Q-MID-002 | LedBar | 故障显示是否需要落地 | `LedBar.c:702-717`, `LedBar.c:1022-1024` | 有故障判断但显示分支为空 | MISUNDERSTOOD | 用户端故障提示缺失 | LED 是否只显示 SOC，还是也要显示 fault pattern？ | A/B/C/F | |
| Q-MID-003 | SOC | 初始 SOC 默认 60% 是否符合体验 | `SocEnhance.c:609-668` | Snapshot 失效后 OCV/default 初始 | UNKNOWN | 首次上电显示偏差 | 新板默认 SOC 应来自 OCV、固定值、工厂写入？ | A/C/F | |
| Q-MID-004 | SOC | SOC runtime table 是否永久关闭 | `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE 0`, `Sci_Upper.c:1860-1884` | `0x2200` SOC 表写入被拒绝 | MAYBE_UNUSED | 上位机旧功能可能不可用 | 上位机是否还需要改 SOC 表？ | A/D/E/F | |
| Q-MID-005 | 校准 | Modbus 校准写入是否已废弃 | `Sci_WrRegs_0x10_CalibCoef()` 主体 `#if 0` | 地址保留但写入无效 | MAYBE_UNUSED | 旧上位机校准入口可能失败 | 量产是否还通过协议写 K/B？ | B/D/F | |
| Q-MID-006 | RTC 参数 | RTC 写寄存器是否需要支持 | `Sci_WrRegs_0x10_RTC()` 空函数 | 读 RTC 有，写 RTC 无效 | MAYBE_UNUSED | 时间设置入口不一致 | 上位机是否要设置 RTC？ | A/C/D/F | |
| Q-MID-007 | 铜损 | 铜损表是否仍参与业务 | `Sci_WrRegs_0x10_CopperLoss()` 空函数；`Sci_ACK_0x03_RW_Data_Other()` 仍读 | 可读但写入无效 | MAYBE_UNUSED | 历史参数占用协议空间 | 铜损是否删除为占位？ | A/D/E/F | |
| Q-MID-008 | CAN 版本 | CAN 周期帧版本号是否应来自产品信息 | `CanFeidaoFrames.c:158-166` | 固定 `pro_version=1`, `soft_version=1` | CHANGE_NEEDED | 客户诊断版本错误 | CAN 版本字段应映射 `FD_VERSION` 还是 `0xC002`？ | B/C/F | |

## 3. 可以后续确认

| ID | 模块 | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 风险 | 需要我确认的问题 | 建议选项 | 我的决定 |
|---|---|---|---|---|---|---|---|---|---|
| Q-LATER-001 | 调试 | `PROJECT_CFG_DEBUG_WATCH_ENABLE` 默认关闭但保留观察指针 | `LedBar.c:146-148`, `Can_HDX.c:6-12` | Debug watch 编译期开关 | KEEP_BUT_REFACTOR | 低 | 是否保留统一 debug watch 规范？ | A/B/E | |
| Q-LATER-002 | Flash 测试 | 后 64K 快速/使用测试是否保留 | `Project_Config.h:199-223` | 当前关闭 | MAYBE_UNUSED | 低/中 | 作为工厂诊断保留还是移到独立测试固件？ | B/D/E | |
| Q-LATER-003 | SCI2/SCI3 | 模板是否需要保留多串口上位机入口 | `Project_Config.h:185-193`, `Sci_Upper.c:1738-1766` | 当前 SCI2/SCI3 关闭 | KEEP_BUT_REFACTOR | 低 | 后续 F0/F1 模板是否需要多串口？ | A/B/D/E | |
| Q-LATER-004 | 日志 | EasyLogger 串口日志在 Release 默认关闭是否固定 | `Project_Config.h:147-149` | 后定义为 0 | MUST_KEEP | 低 | 是否允许 Debug 构建打开日志？ | A/B/E | |

## 4. 可能删除的历史需求

| ID | 模块 | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 风险 | 需要我确认的问题 | 建议选项 | 我的决定 |
|---|---|---|---|---|---|---|---|---|---|
| Q-DEL-001 | Heat/Cool | 加热/冷凝模块是否已移除 | 工程未列入 `Heat_Cool.c/IODrivers.c`，历史文档显示已删除 | 只剩协议/状态残留可能存在 | REMOVE_CANDIDATE | 协议兼容 | 是否彻底归档旧需求，只保留协议占位？ | D/E/F | |
| Q-DEL-002 | 外部 EEPROM | 外部 EEPROM 是否彻底不再使用 | `EEPROM.c:313-337` 旧函数空实现 | 参数已迁移内部 Flash | REMOVE_CANDIDATE | 命名兼容 | 是否允许把文档称为“EEPROM 兼容层”？ | B/C/E | |
| Q-DEL-003 | 旧 CAN 低功耗发射状态机 | 当前 `Can_HDX.c` 已是队列+周期服务 | 多份旧文档描述上电/断电状态机 | REMOVE_CANDIDATE | 文档误导 | 是否归档旧 CAN 低功耗调度方案？ | D/E | |
| Q-DEL-004 | SOC 运行态表/铜损表 | 宏关闭或写函数空 | 保留读/地址 | REMOVE_CANDIDATE | 上位机旧功能 | 这些地址是否仅协议占位？ | D/E/F | |

## 5. 可能理解错误的需求

| ID | 模块 | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 风险 | 需要我确认的问题 | 建议选项 | 我的决定 |
|---|---|---|---|---|---|---|---|---|---|
| Q-MIS-001 | LedBar 图标 | 充电图标当前由 `DSG_FET` 放电 MOS 状态决定 | `LedBar.c:1014-1020` | 放电 MOS open 点亮 charge icon | MISUNDERSTOOD | UI 语义错误 | 图标到底表示充电、输出、放电 MOS，还是唤醒？ | A/C/F | |
| Q-MIS-002 | 老化计时 | 老化只累计 MCU 运行时间，STOP 不计 | `FactoryAging.c:345-357` | 睡眠时间不计老化 | UNKNOWN | 工厂时长偏差 | 老化要求自然时间还是运行通电时间？ | A/C/F | |
| Q-MIS-003 | Type-C 电流 | PA2 Type-C 输出电流并入 SOC | `SOC.c:104-172` | 输出侧折算为电池侧放电 | UNKNOWN | SOC 偏差 | Type-C 输出是否一定来自电池，应不应该扣 SOC？ | A/C/F | |

## 5.1 BMS App IO 与 RTC 低功耗专项确认问题

状态：部分验证

参考源码：

- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/conf/conf_gpio.h`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/Can_HDX.c`

| ID | 模块 | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 风险 | 需要我确认的问题 | 建议选项 | 我的决定 |
|---|---|---|---|---|---|---|---|---|---|
| Q-RTC-IO-001 | IO/AFE | `PB0 / AFE1_PRO_EN` 是否需要在 RTC STOP 唤醒后显式恢复 | `conf.c:InitIO()`, `conf.c:InitIO_rtc()` | 正常初始化配置 PB0，唤醒恢复未显式配置 | UNKNOWN | AFE 保护/供电状态可能不确定 | PB0 的真实硬件功能是什么，唤醒后必须置成什么状态？ | F. 补充原理图；B. 确认后补恢复代码；E. 暂不改继续实测 | |
| Q-RTC-IO-002 | IO/低功耗 | `PA3 / 2737_EN` 休眠时是否必须保持非模拟状态 | `conf.c:IOstatus_RTCMode()` | RTC 模式 GPIOA 模拟化时排除 PA3 | UNKNOWN | 可能增加休眠电流或影响硬件保持 | PA3 休眠时应保持输出、拉低，还是模拟输入？ | F. 补充硬件要求；B. 保持并文档化；C. 改休眠状态 | |
| Q-RTC-IO-003 | IO/AFE | `PB14 / AFE1_CTL` 休眠时是否必须保持非模拟状态 | `conf.c:IOstatus_RTCMode()` | RTC 模式 GPIOB 模拟化时排除 PB14 | UNKNOWN | 可能影响 AFE 控制或漏电 | PB14 休眠时应保持输出、拉低，还是模拟输入？ | F. 补充硬件要求；B. 保持并文档化；C. 改休眠状态 | |
| Q-RTC-CAN-001 | CAN/低功耗 | RTC 周期唤醒后是否必须短时上电 CAN 广播 | `Can_HDX.c`, `rtc_sleep.c` | 当前保留 RTC wake CAN 服务策略 | UNKNOWN | 提高功耗，但增强休眠通信可见性 | 休眠中需要周期 CAN 可见，还是只在外部唤醒后通信？ | A. 保留；C. 改为更省电；E. 暂保留待实测 | |

## 6. 高风险需求

| ID | 模块 | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 风险 | 需要我确认的问题 | 建议选项 | 我的决定 |
|---|---|---|---|---|---|---|---|---|---|
| Q-RISK-001 | IAP | 禁止裸写 App bin 到 `0x08000000` | `Flash.h:4-5`, `tools/soc_flash_app_safe.ps1:17-20` | 安全脚本强制 `0x08004800` | MUST_KEEP | P0 覆盖 IAP | 后续是否把所有烧录文档只指向安全脚本？ | A/B | |
| Q-RISK-002 | AFE MTP | 上位机写参数可能触发 AFE MTP/ROM 写入 | `SH367309_DataDeal.c:145-249` | 差异写入并 reset AFE | MUST_KEEP/UNKNOWN | P0/P1 | 是否需要工装模式保护？ | B/C/F | |
| Q-RISK-003 | 低功耗 | fault active 阻塞 sleep | `app_lowpower.c:59-61` | 有 fault 不进 STOP | MUST_KEEP | P1 | 是否所有 fault 都阻塞低功耗，还是过放必须允许 deep sleep？ | A/C/F | |
| Q-RISK-004 | App 地址 | 工程 XML 地址口径不单一 | `uvprojx` 搜索 `IROM` 结果 | 需要 map/bin 验证 | UNKNOWN | P0 | 后续是否把链接地址检查做进脚本门禁？ | B/C | |
