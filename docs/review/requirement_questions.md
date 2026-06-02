# 103-309 BMS 需求合理性判断与确认问题表

> 本表以当前源码为第一依据。分类含义：`MUST_KEEP` 必须保留；`KEEP_BUT_REFACTOR` 保留需求但可改实现；`CHANGE_NEEDED` 需求或实现需要修改；`MAYBE_UNUSED` 可能不用；`MISUNDERSTOOD` 可能理解错；`CONFLICT` 与其他需求冲突；`UNKNOWN` 必须用户确认；`REMOVE_CANDIDATE` 可考虑删除但不能直接删。

## 1. 必须先确认，否则不能重构

| ID | 模块 | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 风险 | 需要我确认的问题 | 建议选项 | 我的决定 |
|---|---|---|---|---|---|---|---|---|---|
| Q-CRIT-001 | AFE/电流/SOC | 量产固件应使用真实 AFE CADC 电流，还是允许测试虚拟电流 | `DataDeal.c:1238-1239` 注释 `DataLoad_Current()`，调用 `test_Autocurrent_cycle()`；`Project_Config.h:17` 为 profile 0 | 200ms 主路径实际跑虚拟充放电循环，仍递增 `g_u32AfeCurrentSampleSeq` | CHANGE_NEEDED | P0：SOC、CAN 电流、保护状态和老化行为都可能虚假 | 当前量产是否必须恢复 `DataLoad_Current()` 并把虚拟电流放入测试 profile？ | C. 修改需求，按真实电流实现；E. 暂时保留但禁止出货；F. 补充背景 | |
| Q-CRIT-002 | 均衡 | 当前产品是否需要主动均衡 | 仅找到 `OtherElement.u16Balance_*`、`MTP_BALANCEH/L`、CBC status；未见 `App_CellBalance()` 进入 `Runtime_RunOnce()` | 有均衡参数和状态位，但未确认主动控制 | UNKNOWN | P0/P1：若客户要求均衡，当前实现可能缺功能；若不要求，协议残留增加复杂度 | 均衡是当前客户需求、未来模板需求，还是历史残留？ | A. 保留原需求；B. 保留并重构；D. 删除业务但保留协议占位；F. 补充背景 | |
| Q-CRIT-003 | AFE 参数 | 均衡开启电压是否应使用可写参数 | `SH367309_DataDeal.c:58-59` 注释参数计算，实际硬编码 `4160` | 上位机写 `u16Balance_OpenVoltage` 可能不影响 AFE ROM 对应字段 | MISUNDERSTOOD | P1：上位机参数和硬件行为不一致 | `u16Balance_OpenVoltage` 是否必须驱动 AFE 均衡开压？ | B. 保留需求但修实现；C. 修改为固定 4160 并文档化；F. 补充背景 | |
| Q-CRIT-004 | Flash/IAP | 当前真实硬件 Flash 容量和 App 链接地址是什么 | `Flash.h:4-30` 使用 `0x08004800` App 和 `0x0801C000+` 存储；Keil XML 同时有 `0x08000000` 和 `0x8004800`，`ScatterFile` 为空 | 安全脚本约束 App 烧录 `0x08004800`，但 Keil 工程显示不够单一 | UNKNOWN | P0：地址错会覆盖 IAP 或越界写 Flash | 实际量产芯片是 64KB、128KB 还是 C8 兼容大 Flash？Keil 最终链接地址以哪个文件为准？ | A. 保留现有地址并补 map 验证；C. 修改地址策略；F. 补充硬件/BOM | |
| Q-CRIT-005 | Host 写权限 | 量产固件是否允许上位机写保护/Other/AFE/IAP/SOC | `PROJECT_CFG_HOST_WRITE_ENABLE 1`；`Sci_Upper.c:1818-1984` | 当前量产宏下可写多个关键参数和 IAP 请求 | UNKNOWN | P0/P1：现场误写会影响保护阈值、AFE MTP、IAP 进入 | 量产版本是否需要密码/工装模式/只读模式？ | A. 保留原写权限；B. 保留但加权限层；C. 按新逻辑收紧；F. 补充工厂流程 | |
| Q-CRIT-006 | 低功耗/IWDG | RTC 周期唤醒是否必须兼顾 CAN 周期广播 | `RTC.c`, `rtc_sleep.c`, `Can_HDX.c` | 已确认改为 RTC 周期唤醒不主动发 CAN；睡前关闭 CMNT，唤醒恢复后重新打开 | CHANGE_NEEDED | P1：需要上板确认睡前/唤醒后 CMNT 电平和 CAN 恢复 | 是否接受当前“休眠不广播，唤醒后通信”的策略作为量产默认？ | C. 已按更省电策略修改；后续只做实测验证 | 已确认：RTC 休眠中不周期广播 CAN |
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
| Q-RTC-CAN-001 | CAN/低功耗 | RTC 周期唤醒后是否必须短时上电 CAN 广播 | `Can_HDX.c`, `rtc_sleep.c`, `RTC.c` | 已删除 RTC wake CAN 服务；CMNT 睡前关闭，唤醒恢复后打开 | CHANGE_NEEDED | 休眠中 CAN 不再周期可见，但功耗更低 | 休眠中需要周期 CAN 可见，还是只在外部唤醒后通信？ | C. 改为更省电 | 已确认：只在唤醒恢复后通信 |

## 6. 高风险需求

| ID | 模块 | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 风险 | 需要我确认的问题 | 建议选项 | 我的决定 |
|---|---|---|---|---|---|---|---|---|---|
| Q-RISK-001 | IAP | 禁止裸写 App bin 到 `0x08000000` | `Flash.h:4-5`, `tools/soc_flash_app_safe.ps1:17-20` | 安全脚本强制 `0x08004800` | MUST_KEEP | P0 覆盖 IAP | 后续是否把所有烧录文档只指向安全脚本？ | A/B | |
| Q-RISK-002 | AFE MTP | 上位机写参数可能触发 AFE MTP/ROM 写入 | `SH367309_DataDeal.c:145-249` | 差异写入并 reset AFE | MUST_KEEP/UNKNOWN | P0/P1 | 是否需要工装模式保护？ | B/C/F | |
| Q-RISK-003 | 低功耗 | fault active 阻塞 sleep | `app_lowpower.c:59-61` | 有 fault 不进 STOP | MUST_KEEP | P1 | 是否所有 fault 都阻塞低功耗，还是过放必须允许 deep sleep？ | A/C/F | |
| Q-RISK-004 | App 地址 | 工程 XML 地址口径不单一 | `uvprojx` 搜索 `IROM` 结果 | 需要 map/bin 验证 | UNKNOWN | P0 | 后续是否把链接地址检查做进脚本门禁？ | B/C | |
