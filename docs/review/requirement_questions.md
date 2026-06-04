# 103-309 BMS 需求合理性判断与确认问题表

> 本表以当前源码为第一依据。分类含义：`MUST_KEEP` 必须保留；`KEEP_BUT_REFACTOR` 保留需求但可改实现；`CHANGE_NEEDED` 需求或实现需要修改；`MAYBE_UNUSED` 可能不用；`MISUNDERSTOOD` 可能理解错；`CONFLICT` 与其他需求冲突；`UNKNOWN` 必须用户确认；`REMOVE_CANDIDATE` 可考虑删除但不能直接删。

## 1. 必须先确认，否则不能重构

| ID | 模块 | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 风险 | 需要我确认的问题 | 建议选项 | 我的决定 |
|---|---|---|---|---|---|---|---|---|---|
| Q-CRIT-001 | AFE/电流/SOC | 量产固件应使用真实 AFE CADC 电流，测试虚拟电流必须隔离 | 当前 200ms 主路径调用 `DataLoad_Current()`，采样序号通过 `AfeCurrent_GetSeq()` 暴露；`Project_Config.h` 为 profile 0 | 当前源码主路径已是 `DataLoad_Current()`，旧文档中“主路径跑虚拟电流”的描述已过期；仍需确认测试注入入口是否完全隔离 | MUST_KEEP | P0：若虚拟电流再次混入量产，会影响 SOC、CAN 电流、保护状态和老化行为 | 是否确认量产必须保持 `DataLoad_Current()`，虚拟电流只能在测试 profile/测试固件使用？ | A. 保持当前真实电流主路径；B. 保留测试入口但加门禁；F. 补充背景 | |
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
| Q-MID-004 | SOC | SOC runtime table 是否永久关闭 | `SocEnhance.c`, `Sci_Upper.c`, `EEPROM.c` | runtime table 宏、数组、EEPROM 默认表装载和写表分支已删除；`0x2200` 写 SOC 表固定拒绝 | 已确认 | 上位机旧写表入口不可用，但量产算法更简单 | 是否接受删除 runtime table？ | 已执行删除 | 用户已确认：可以删除 |
| Q-MID-005 | 校准 | Modbus 校准写入是否已废弃 | `Sci_WrRegs_0x10_CalibCoef()` 主体 `#if 0` | 地址保留但写入无效 | MAYBE_UNUSED | 旧上位机校准入口可能失败 | 量产是否还通过协议写 K/B？ | B/D/F | |
| Q-MID-006 | RTC 参数 | RTC 写寄存器是否需要支持 | `Sci_WrRegs_0x10_RTC()` 空函数 | 读 RTC 有，写 RTC 无效 | MAYBE_UNUSED | 时间设置入口不一致 | 上位机是否要设置 RTC？ | A/C/D/F | |
| Q-MID-007 | 铜损 | 铜损表是否仍参与业务 | `Sci_WrRegs_0x10_CopperLoss()` 空函数；`Sci_ACK_0x03_RW_Data_Other()` 仍读 | 可读但写入无效 | MAYBE_UNUSED | 历史参数占用协议空间 | 铜损是否删除为占位？ | A/D/E/F | |
| Q-MID-008 | CAN 版本 | CAN 周期帧版本号是否应来自产品信息 | `CanFeidaoFrames.c:158-166` | 固定 `pro_version=1`, `soft_version=1` | CHANGE_NEEDED | 客户诊断版本错误 | CAN 版本字段应映射 `FD_VERSION` 还是 `0xC002`？ | B/C/F | |
| Q-MID-009 | SOC | mid-tail 是否删除，静置 OCV 是否只保留长静置慢速下修 | `SocEnhance.c`, `SocEnhance.h`, `tools/soc_replay_test.py` | mid-tail 表/计数/debug/test 已删除；短静置 deferred OCV 已删除；只保留长静置慢速下修 | 已确认 | 降低静置/中段快降和隐藏目标消化复杂度；低端虚高主要由 low-tail 控制 | 是否接受删除 mid-tail 和 short-rest/deferred OCV？ | 已执行删除 | 用户已确认：都可以 |

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

状态：部分验证，2026-06-02 已按当前源码追加低功耗简化确认项

参考源码：

- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/conf/conf_gpio.h`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/System_Init.c`
- `103 + 309/Project/Source/SleepDeal.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/FactoryAging.c`

| ID | 模块 | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 风险 | 需要我确认的问题 | 建议选项 | 我的决定 |
|---|---|---|---|---|---|---|---|---|---|
| Q-RTC-IO-001 | IO/AFE | `PB0 / AFE1_PRO_EN` 是否需要在 RTC STOP 唤醒后显式恢复 | `conf.c:InitIO()`, `conf.c:InitIO_rtc()` | 正常初始化配置 PB0，唤醒恢复未显式配置 | UNKNOWN | AFE 保护/供电状态可能不确定 | PB0 的真实硬件功能是什么，唤醒后必须置成什么状态？ | F. 补充原理图；B. 确认后补恢复代码；E. 暂不改继续实测 | |
| Q-RTC-IO-002 | IO/低功耗 | `PA3 / 2737_EN` 休眠时是否必须保持非模拟状态 | `conf.c:IOstatus_RTCMode()` | RTC 模式 GPIOA 模拟化时排除 PA3 | UNKNOWN | 可能增加休眠电流或影响硬件保持 | PA3 休眠时应保持输出、拉低，还是模拟输入？ | F. 补充硬件要求；B. 保持并文档化；C. 改休眠状态 | |
| Q-RTC-IO-003 | IO/AFE | `PB14 / AFE1_CTL` 休眠时是否必须保持非模拟状态 | `conf.c:IOstatus_RTCMode()` | RTC 模式 GPIOB 模拟化时排除 PB14 | UNKNOWN | 可能影响 AFE 控制或漏电 | PB14 休眠时应保持输出、拉低，还是模拟输入？ | F. 补充硬件要求；B. 保持并文档化；C. 改休眠状态 | |
| Q-RTC-CAN-001 | CAN/低功耗 | RTC 周期唤醒后是否必须短时上电 CAN 广播 | `Can_HDX.c`, `rtc_sleep.c`, `RTC.c` | 已删除 RTC wake CAN 服务；CMNT 睡前关闭，唤醒恢复后打开 | CHANGE_NEEDED | 休眠中 CAN 不再周期可见，但功耗更低 | 休眠中需要周期 CAN 可见，还是只在外部唤醒后通信？ | C. 改为更省电 | 已确认：只在唤醒恢复后通信 |
| Q-RTC-LP-001 | 低功耗/IWDG | `PROJECT_CFG_WDOG_ENABLE` 是否必须真实控制 IWDG | `Project_Config.h:58`, `AppInit.c:32`, `System_Init.c:37-52` | 当前默认 1；`Init_IWDG()` 和 `IWDG_Feed()` 已按宏门控 | 已处理 | IWDG 开启会限制 RTC wake 周期，功耗目标需实测确认 | 是否接受量产稳定优先、默认启用 IWDG？ | B. 统一宏和实际行为，建议量产启用 | 本轮已处理 |
| Q-RTC-LP-002 | 低功耗/调试 | Release 是否必须关闭 DBGMCU 低功耗调试保持 | `conf.h`, `System_Init.c:21-34`, `docs/review/rtc_sleep_low_power_requirement_confirmation_2026-05-27.md` | 已删除无条件 `__EnableLowPowerDebug__`，Release 默认清除 DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP | 已处理 | STOP 功耗实测必须避免调试保持位 | 是否继续保持量产构建关闭 DBGMCU 低功耗调试保持？ | C. Release 关闭，只允许 Debug/显式宏打开 | 已确认并已处理 |
| Q-RTC-LP-003 | 低功耗/老化 | 工厂老化 active 是否阻塞 STOP | `FactoryAging.c:422-436`, `rtc_sleep.c`, `rtc_sleep.h` | 已按确认接入：老化 running 只阻塞 HICCUP idle 进入 RTC STOP，不阻塞低压或外部请求的 `DEEP_MODE/NORMAL_MODE` reset sleep | 已处理 | 老化计时不会被 RTC STOP 打断，同时不影响深睡/保护路径 | 是否保持“老化只不允许进入 RTC”？ | B. 保持当前窄范围实现 | 已确认：只是不允许进入 RTC |
| Q-RTC-LP-004 | 低功耗/AFE | AFE not idle 是否阻塞 HICCUP STOP | `rtc_sleep.c`, `rtc_sleep_afe_sh367309.c`, `rtc_sleep.h` | 当前主判断不检查 AFE not idle；框架层未触发的 `LP_BLOCK_AFE_BUSY` 和未使用 wrapper 已删除 | CONFLICT | AFE 异常、保护或 PCHG 状态下可能进入 STOP | SH367309 哪些状态必须禁止 HICCUP STOP？ | B. 确认后恢复最小 RTC block 或删除保留 reason | |
| Q-RTC-LP-005 | 低功耗/AFE | HICCUP STOP 前是否需要让 AFE 进入 sleep | `rtc_sleep_port.c:92-100`, `SleepDeal.c:109-114`, `SH367309_Func.c:65-70` | Reset sleep 前调 `AFE_Sleep()`；HICCUP STOP 前当前未直接调 | UNKNOWN | 可能影响 STOP 电流，也可能影响周期测量恢复 | HICCUP 期间 AFE 应保持可快速测量，还是进入 AFE sleep？ | F. 结合 SH367309 手册和实测确认 | |
| Q-RTC-LP-006 | 低功耗/参数 | `OtherElement` 普通休眠和 RTC 参数是否仍有效 | `DataDeal.h:116-123`, `rtc_sleep.c:152-159`, `RTC.c:369-393` | 当前只用低压阈值/低压时间；RTC 周期默认 10 秒 | CHANGE_NEEDED | 上位机写入参数可能不影响真实低功耗行为 | `u16Sleep_VNormal/TimeNormal/RTC_WakeUpTime/TimeRTC` 是保留、接入还是删除？ | C. 无真实需求则文档化占位，后续删除误导 | |
| Q-RTC-LP-007 | 低功耗/交互 | 睡眠中短按显示 SOC、长按约 500ms 开机是否是产品定义 | `SleepDeal.c:22-80`, `LedBar.c:1209-1218` | 短按只预览，充电或长按才退出 sleep | UNKNOWN | 用户体验不清，可能误以为按键无效 | 睡眠中按键行为是否确认？ | A. 保留；C. 调整按键时长/逻辑；F. 补充产品定义 | |
| Q-RTC-LP-008 | 低功耗/交互 | 运行态长按约 500ms 是否直接 DEEP_MODE 关机 | `LedBar.c:987-999` | 长按后直接 `SleepDeal_Continue(DEEP_MODE)` | UNKNOWN | 可能误关机 | 运行态长按关机时间和行为是否正确？ | B. 保留但把时间参数化/文档化；C. 修改 | |
| Q-RTC-LP-009 | 低功耗/充电 | 充电器拔除后是否直接进入 DEEP_MODE | `DataDeal.c:51-95` | `CHG_IN` 释放后请求 `DEEP_MODE` | UNKNOWN | 拔 5V 后关机/待机/继续运行体验不同 | 当前产品拔 5V 后目标行为是什么？ | F. 需要产品定义 | |
| Q-RTC-LP-010 | 低功耗/简化 | 未使用的 `LP_EnterStop/LP_BeforeSleep/LP_AfterWakeup/LP_SetWakeupPeriod/LP_Task` 是否可删除 | `rg` 仅见旧文档引用，源码主路径实际走 `Runtime_RunOnce()->rtc_sleep()` | 已删除未使用 wrapper、`LP_State_t` 和无消费者状态缓存 | 已处理 | 若外部工具依赖旧 API，需要同步工具；源码主路径不变 | 是否允许后续继续按“无调用、无协议、无硬件行为影响”净删减？ | D. 删除或改 static，保持主路径不变 | 本轮已处理 |

## 5.2 状态变量净删减专项确认问题

状态：部分验证，2026-06-02 按当前源码新增

专项文档：`docs/review/state_variable_audit.md`

参考源码：

- `103 + 309/Project/Source/main.c`
- `103 + 309/Project/Source/AppInit.c`
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/LogRecord.c`
- `103 + 309/Project/Source/System_Monitor.c`
- `103 + 309/Project/Source/DataDeal.c`
- `103 + 309/Project/Source/SOC.c`
- `103 + 309/Project/Source/ProductionID.c`
- `103 + 309/Project/Source/FactoryAging.c`
- `103 + 309/Project/Source/LogRecord.c`

| ID | 模块 | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 风险 | 需要我确认的问题 | 建议选项 | 我的决定 |
|---|---|---|---|---|---|---|---|---|---|
| Q-SV-001 | LedBar/初始化 | `s_ledbar.initialized` 是否改为显式初始化后删除大部分懒初始化判断 | `AppInit.c:36-45`, `LedBar.c:171-177`, `LedBar.c:1034-1067`, `LedBar.c:1299-1368`, `conf/conf.c:273-349` | 已把 `LedBar_Init()` 放入启动运行态初始化；`APP_LedBar()` 不再懒初始化；外部 API、STOP 前 GPIO、TIM4 ISR/debug 仍保留 `initialized` 防护 | 已处理，保留安全保护 | 继续删除不当会导致 LED 重复初始化、TIM4 ISR 未初始化访问或 STOP 前 GPIO 状态异常；RTC STOP 唤醒后不应完整重置 LedBar runtime | 是否允许把 `LedBar_Init()` 放入启动流程，先保留 ISR 保护，再分批删除 `LedBar_EnsureInit()`？ | 已执行；不改 RTC 睡前/唤醒恢复链 | 已执行 |
| Q-SV-002 | 低功耗 | `readyToSleep` 是否作为重复阶段变量删除/收口 | `rtc_sleep.c`, `rtc_sleep.h`, `LedBar.c`, `LogRecord.c`, `Runtime.c`, `SystemDebug.c`, `tools/stlink_bms_monitor.ps1` | 已删除 `readyToSleep` 字段和 `LowPower_IsToSleepPending()/LowPower_ClearToSleepFlag()`；`rtc_sleep()` 用本地 `sleep_mode` 提交；debug/ST-Link ready 由 `mode != NO_SLEEP` 派生 | 已处理 | 若提交点遗漏会漏 sleep SOC、BMS_SLEEP 日志或 debug busy；本批保留 reset sleep 和 HICCUP STOP 准备链 | 是否允许把 `readyToSleep` 改成本地提交决策，并把 LED/日志收尾统一到 sleep commit？ | 已执行；不改 sleep SOC、BMS_SLEEP 和外设恢复顺序 | 已执行 |
| Q-SV-003 | 低功耗/debug | `g_stLowPowerRtcStatus` 中展示字段是否迁到 debug 快照或明确标记为 mirror | `rtc_sleep.h:50-58`, `rtc_sleep.c:86-92`, `SystemDebug.c:536-541` | 控制状态和展示状态混在同一个全局结构体 | KEEP_BUT_REFACTOR | 维护者容易误以为 `rtcWake/delay/elapsed` 参与控制 | 是否允许先文档标记，后续把纯展示字段从控制结构里移出？ | B. 保留可观测性但降低控制耦合 | |
| Q-SV-004 | 产品信息 | `ProductionID.c` 的 `su8_StartUpFlag` 是否可由启动流程替代 | `ProductionID.c`, `ProductionID.h`, `AppInit.c`, `Runtime.c:57` | 已把 `InitProID()` 收口到启动运行态初始化；后台 `App_ProID_Deal()` 不再依赖一次性 flag | 已处理 | 仍需上位机/真板确认 `0xC002` 默认信息读取 | 是否允许把 `InitProID()` 收口到 `AppInit_Boot()`，删除主循环一次性 flag？ | 已执行；保留 PROID heartbeat hook | 已执行 |
| Q-SV-005 | 状态保留边界 | 按键防抖、`MCU_WK` 防抖、SOC sample seq、AFE fault 计数是否作为真实历史状态保留 | `LedBar.c:890-1009`, `SOC.c:116-142`, `DataDeal.c:825-917` | 这些变量承担边沿检测、去重积分、故障持续时间判断 | MUST_KEEP | 误删会导致误唤醒、重复积分、故障恢复失败 | 是否接受本轮净删减边界：只删重复事实和阶段残留，不删真实历史状态？ | A. 保留这些状态，只优化命名/边界 | |
| Q-SV-006 | DataDeal/客户逻辑 | `DataDeal.c` 中充电器、MOS 过温、UL 认证、RF_EN 熔断类状态是否仍是当前产品需求 | `DataDeal.c:51-95`, `DataDeal.c:930-1055` | 多个静态状态混在 200ms 业务链路里 | UNKNOWN | 直接删除可能改变安全输出、认证动作或客户体验 | 这些逻辑是当前产品需求、认证需求，还是历史残留？ | F. 先补产品/认证背景，不进第一批删除 | |
| Q-SV-007 | 老化/状态收口 | `FactoryAging.c` 中同生命周期私有变量是否可集中为模块 runtime 结构体 | `FactoryAging.c:28-37`, `FactoryAging.c:45-627` | 已把老化 state、elapsed、last tick、保存节流、finish retry、duration hours 和 MOS mode 收口到 `FactoryAgingRuntime s_factory_aging` | 已处理 | 替换错误会影响老化剩余时间、保存进度、完成重试或 MOS 模式缓存 | 是否允许对单文件私有运行态做结构体收口，提升 Keil Watch 可读性？ | 已执行；不改状态机和持久化格式 | 已执行 |
| Q-SV-008 | 日志/状态收口 | `LogRecord.c` 中私有日志运行态是否可集中为模块 runtime 结构体 | `LogRecord.c`, `LogRecord.h`, `rtc_sleep_port.c` | 已把日志 point、records、startup/sleep flag、uptime、重复保存抑制和事件 latch 收口到 `LogRecordRuntime s_log_record`；保留外部 `su32_Interval_S_Tcnt` | 已处理 | 误搬外部时间符号会影响 RTC 睡眠补偿；误改 latch 会改变事件去重 | 是否允许先收口私有状态，保留跨模块时间累计符号？ | 已执行；不改日志格式、Flash 保存格式和低功耗补偿接口 | 已执行 |
| Q-SV-009 | AFE/电流 | `DataDeal.c` 中 AFE current zero 私有状态是否可集中为 runtime 结构体 | `DataDeal.c`, `DataDeal.h`, `SOC.c` | 已把启动零点、zero offset、last raw、stable count、ready、zero state 和采样序号收口到 `DATA_RUNTIME s_data`；外部通过 `AfeCurrent_GetSeq()` 读取 | 已处理 | 替换错误会影响零点、自学习、电流方向和 SOC 积分 | 是否允许对 AFE current zero 私有状态做结构体收口，保留 CADC/换算公式/deadband/sample seq？ | 已执行；不改算法和采样序号接口 | 已执行 |

## 6. 高风险需求

| ID | 模块 | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 风险 | 需要我确认的问题 | 建议选项 | 我的决定 |
|---|---|---|---|---|---|---|---|---|---|
| Q-RISK-001 | IAP | 禁止裸写 App bin 到 `0x08000000` | `Flash.h:4-5`, `tools/soc_flash_app_safe.ps1:17-20` | 安全脚本强制 `0x08004800` | MUST_KEEP | P0 覆盖 IAP | 后续是否把所有烧录文档只指向安全脚本？ | A/B | |
| Q-RISK-002 | AFE MTP | 上位机写参数可能触发 AFE MTP/ROM 写入 | `SH367309_DataDeal.c:145-249` | 差异写入并 reset AFE | MUST_KEEP/UNKNOWN | P0/P1 | 是否需要工装模式保护？ | B/C/F | |
| Q-RISK-003 | 低功耗 | fault active 阻塞 sleep | `rtc_sleep.c:LP_GetBlockReason()` | 有 fault 不进 STOP | MUST_KEEP | P1 | 是否所有 fault 都阻塞低功耗，还是过放必须允许 deep sleep？ | A/C/F | |
| Q-RISK-004 | App 地址 | 工程 XML 地址口径不单一 | `uvprojx` 搜索 `IROM` 结果 | 需要 map/bin 验证 | UNKNOWN | P0 | 后续是否把链接地址检查做进脚本门禁？ | B/C | |

## 7. RTC 唤醒后 ADC 采样收敛与简化确认问题

状态：已确认并已执行，2026-06-04 源码已修改，硬件待验证

专项文档：`docs/review/adc_rtc_wakeup_simplification_2026-06-04.md`

参考源码：

- `103 + 309/Project/Source/ADC.c`
- `103 + 309/Project/Source/ADC.h`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/DataDeal.c`
- `103 + 309/Project/Source/SOC.c`

执行说明：用户已确认直接采样计算方案，本轮已删除 VBC/MOS/Type-C 软件滤波并改为 latest-sample。当前仍需真板验证 ADC 抖动、Type-C 小电流边界和 RTC STOP 唤醒恢复时间。

| ID | 模块 | 需求描述 | 代码证据 | 当前行为 | Codex 判断 | 风险 | 需要我确认的问题 | 建议选项 | 我的决定 |
|---|---|---|---|---|---|---|---|---|---|
| Q-ADC-WAKE-001 | ADC/低功耗 | RTC STOP 前关闭 ADC/TIM2/DMA、唤醒后重新 `InitADC()` 是否必须保留 | `conf/conf.c:114-118`, `conf/conf.c:335-344`, `ADC.c:256-270`, `ADC.c:462-480` | 当前低功耗路径会停 ADC 并在唤醒后重建 | MUST_KEEP | 删除会影响 STOP 功耗和外设恢复确定性 | 是否确认本轮简化不删除 ADC stop/reinit 低功耗路径？ | A. 保留当前低功耗路径 | 已确认并已执行 |
| Q-ADC-WAKE-002 | ADC/VBC | RTC 唤醒后 VBC/MOS 温度是否允许第一组有效样本直接初始化最终值 | `ADC.c:430-431`, `ADC.c:451-456`, `ADC.c:466-475` | 当前从 0 开始 IIR，最终值需要一段时间收敛 | CHANGE_NEEDED | 过早使用异常首样本会输出错误值；不改则唤醒后继续慢 | 是否允许加入丢弃 1 到 2 组 raw 后首包种子化？ | B. 允许首包加速，并保留少量丢弃保护 | 已确认并已执行 |
| Q-ADC-WAKE-003 | ADC/Type-C | Type-C 电流是否从 32 点平均改成直接计算或少点平均 | `ADC.h:9-10`, `ADC.c:377-420`, `SOC.c:20-54` | 当前非零电流约 330ms 级别才输出 | KEEP_BUT_REFACTOR | 直接计算会快，但可能把 ADC 抖动带入 SOC 等效放电 | Type-C 电流优先追求响应速度，还是保留较强滤波？ | B. 改 4/8 点轻量平均；C. 直接计算加死区/限幅；A. 保留 32 点 | 已确认并已执行 |
| Q-ADC-WAKE-004 | ADC/时基 | `App_AnlogCal()` 是否改为 latest-sample 模式，避免未来改 1s 调用后影响结果 | `ADC.c:483-510`, `Runtime.c:43-51`, `todo.md` | 当前依赖 10ms tick catch-up，单次最多补 10 tick | CHANGE_NEEDED | 若调用频率变低，结果更新会变慢或积压 | 是否确认 ADC 计算要和 App 调用频率解耦？ | B. 改为每次按最新 raw 更新一次 | 已确认并已执行 |
| Q-ADC-WAKE-005 | ADC/总压 | ADC VBC 是否只做诊断/Type-C 辅助，不替代 AFE 单体累加总压 | `DataDeal.c:201-224`, `DataDeal.c:980-986` | `u16VCellTotle` 当前由 AFE 单体累加，ADC VBC 放在辅助/调试位置 | MUST_KEEP | 误替代会影响保护、上报和历史协议口径 | 是否确认最终电池总压继续以 AFE 单体累加为准？ | A. 保持 AFE 总压主路径 | 已确认并已执行 |
