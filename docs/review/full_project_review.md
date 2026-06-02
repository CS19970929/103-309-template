# 103-309 BMS 全项目源码 Review

> 审查状态：源码优先，只读审查。
> 修改范围：本轮仅新增/更新 `docs/review` 与后续文档体系 Markdown，不修改源码、Keil 工程、编译配置和协议行为。
> 当前结论不是重构指令；所有涉及业务行为、客户协议、硬件动作和量产流程的修改必须先经过需求确认。

## 1. 项目整体评价

当前工程已经从旧式“模块散乱轮询”演进成一个可运行的 BMS App 框架：`main -> AppInit_Boot -> Runtime_RunOnce` 主线清晰，TIM3 10ms tick 提供软调度，AFE/SOC 200ms 数据链、Modbus/CAN 上位机兼容、内部 Flash 参数持久化、RTC 低功耗、IWDG、LedBar 和工厂老化都已经接入。

但它还不是一个干净的可复用模板。核心问题不是单个函数写得复杂，而是多个高风险需求叠在同一条数据总线上：`g_stCellInfoReport` 同时服务保护、SOC、CAN、Modbus、LED、低功耗和日志；`DataDeal.c` 同时承担采集、保护辅助、客户逻辑和睡眠触发；通信写参数直接牵动 Flash、SOC、AFE MTP 和 IAP。后续重构必须先确认需求，再分阶段收口。

## 2. 当前架构优点

1. 主入口和运行调度已经集中：`main.c` 只负责启动和循环，`Runtime.c` 明确三段任务顺序。
2. 参数存储已经基本迁移到内部 Flash，并采用双槽/journal、CRC 和写后校验思路。
3. SOC 模块已有较完整的用户体验策略：满电锚点、低压尾段、sag holdoff、静置 OCV、显示平滑和休眠补偿。
4. Modbus 地址表和 CAN App 服务已经能复用同一套寄存器读写逻辑，兼容性边界比直接分散写更好。
5. 低功耗已经有 `app_lowpower + rtc_sleep + port` 分层雏形，能显式阻塞通信、Flash、fault、LED 和 IWDG 不安全周期。
6. IAP 安全规则已经在源码常量和 `tools/soc_flash_app_safe.ps1` 中体现，避免 App 裸烧到 `0x08000000`。

## 3. 当前架构主要问题

1. 需求边界不清：工厂老化、均衡、Type-C 电流、CAN RTC 周期广播、长按休眠、UL/RF_EN 逻辑都可能是客户定制或阶段性调试需求。
2. 模块职责交叉：`DataDeal.c` 既是采集层，又直接控制 MOS、sleep、RF_EN 和 SOC 输入。
3. 协议与业务耦合深：`Sci_Upper.c` 写寄存器后直接触发 Flash 保存、AFE 参数更新、SOC 重载和 IAP。
4. AFE 抽象不足：当前固定 SH367309，长期目标中的“不同 AFE 驱动切换”尚未形成稳定接口。
5. 文档体系分散：根目录和 `docs/` 下存在大量历史方案、阶段记录、低功耗重复设计和通信工具文档，必须按源码重新归并。

## 4. 严重风险

| 风险 ID | 内容 | 源码证据 | 影响 | 建议 |
|---|---|---|---|---|
| R-P0-001 | 量产 profile 下 AFE 200ms 主路径必须保持真实 `DataLoad_Current()`，测试虚拟电流不能回流 | `DataDeal.c:1063-1085`, `Project_Config.h:17` | 若测试虚拟电流再次混入量产，会影响 SOC、CAN 电流、保护显示、老化和低功耗判定 | 当前源码主路径已调用 `DataLoad_Current()`；后续必须把虚拟电流隔离规则作为门禁保留 |
| R-P0-002 | App/IAP/Flash 地址口径需要最终 map 验证 | `Flash.h:4-30`, `tools/soc_flash_app_safe.ps1:17-20`, Keil XML 中同时有 `0x08000000` 与 `0x8004800` | 错烧可能覆盖 IAP；后 64K 写入可能越界 | 所有烧录必须走安全脚本；后续增加 map/bin 地址门禁 |
| R-P0-003 | 均衡参数存在但主动均衡入口未确认 | `DataDeal.h:141-149`, `Sci_Upper.c:1937-1940`, 未见主循环主动均衡任务 | 若产品需要均衡，则功能缺失；若不需要，协议残留误导 | 必须确认均衡需求归属 |
| R-P0-004 | Host 写权限在量产开启，且可能触发 AFE/Flash/IAP | `PROJECT_CFG_HOST_WRITE_ENABLE 1`, `Sci_Upper.c:314-737`, `SH367309_DataDeal.c:145-249` | 现场误写可能改变保护阈值或进入 IAP | 需求确认前不要改协议；后续考虑工装权限层 |

## 5. 中等风险

| 风险 ID | 内容 | 源码证据 | 影响 | 建议 |
|---|---|---|---|---|
| R-P1-001 | AFE 均衡开压硬编码为 4160，参数写入可能不生效 | `SH367309_DataDeal.c:58-59` | 上位机参数和真实 AFE 行为不一致 | 确认后修正或明确固定策略 |
| R-P1-002 | `new_todo_logi()` 内存在客户/认证/临时逻辑混合 | `DataDeal.c:1086-1223` | 后续重构容易误删或误保留 | 先把业务背景确认成表 |
| R-P1-003 | CAN 版本周期帧固定 `1`，不一定等于产品版本 | `CanFeidaoFrames.c:158-166` | 客户诊断版本不可信 | 确认协议字段来源 |
| R-P1-004 | IWDG 限制 RTC wake period 最大 10s，影响低功耗目标 | `RTC.c:386-390` | STOP 频繁唤醒，功耗偏高 | 硬件实测后决定 IWDG/RTC 策略 |
| R-P1-005 | ENV2/ENV3 温度强制 -40 | `DataDeal.c:227-277` | 上位机可能显示误导 | 确认探头数量和无效值规范 |
| R-P1-006 | 产品 ID 默认值像占位字符串 | `DataDeal.h:182-187` | 量产追溯错误 | 需要生产写入流程 |

## 6. 低风险优化点

1. 只读整理文档索引、模块地图、协议地址表和测试计划。
2. 增加源码验证状态头，明确哪些文档是 CURRENT，哪些是历史方案。
3. 为 Modbus/CAN 地址表生成权威协议文档，不改协议行为。
4. 将 `EEPROM` 命名解释为兼容层，避免误以为还有外部 EEPROM。
5. 将低功耗多份阶段文档合并为一个当前设计文档和一个归档索引。

## 7. 不建议马上动的模块

| 模块 | 原因 |
|---|---|
| `Flash.c/.h`, IAP 地址和烧录脚本 | 地址错误代价最高，必须先固化验证门禁 |
| `SH367309_*`, `I2C_AFE1.c` | 直接影响保护、MOS、低功耗唤醒 |
| `Sci_Upper.c` 地址表 | 最容易破坏旧上位机和 PC 工具 |
| `Can_HDX.c`, `CanFeidaoFrames.c` | 已承担老化、IAP、寄存器桥和 RTC wake 服务 |
| `SocEnhance.c` | 算法状态多，用户体验约束强，必须先做测试基线 |
| `SleepDeal.c`, `rtc_sleep.c`, `app_lowpower.c` | 牵涉 IWDG、RTC、BKP、MOS、CAN 和 LedBar |

## 8. 最适合优先重构的模块

| 优先级 | 模块 | 理由 | 前置条件 |
|---|---|---|---|
| 1 | 文档体系和协议/模块索引 | 零源码风险，能降低后续误改概率 | 本轮即可做 |
| 2 | `DataDeal.c` 需求拆分文档 | 先把采集、MOS、客户逻辑、低功耗触发分清 | 需求确认表完成 |
| 3 | Flash/EEPROM 文档和测试脚本门禁 | 不改行为，先锁地址/容量/写频率 | 确认真实 MCU Flash |
| 4 | 通信协议适配层文档化 | 先固定地址表和副作用表 | 上位机兼容确认 |
| 5 | AFE 抽象接口设计 | 可先设计不改代码 | 确认是否保留 SH367309 为第一目标 |

## 9. 最容易破坏协议兼容的模块

1. `Sci_Upper.h/.c`：寄存器地址、窗口长度、异常码、写副作用。
2. `CanFeidaoFrames.c`：周期帧 ID、字段单位、老化剩余时间 `0x14F80208`。
3. `Can_HDX.c`：App 服务命令 `0x60/0x61`、IAP ack、读块流。
4. `ProductionID.c` + `0xC002`：上位机实时监控底栏依赖。
5. SOC 测试窗口 `0xD300`：量产 `supported=0` 是兼容约定。

## 10. 最容易破坏硬件行为的模块

1. `SH367309_Func.c` 和 `SH367309_DataDeal.c`：AFE 保护、MOS、CBC。
2. `I2C_AFE1.c`：AFE 初始化、CADC、startup zero。
3. `conf.c/conf_gpio.h`：GPIO 低功耗状态、唤醒源、电源控制脚。
4. `ADC.c`：ADC/TIM2/DMA 时序、Type-C/VBC/MOS 温度。
5. `LedBar.c`：Charlieplexing GPIO 与 STOP 前引脚状态。

## 11. 当前明显 bug 或疑似 bug

| ID | 严重度 | 描述 | 证据 | 当前判断 |
|---|---|---|---|---|
| BUG-001 | P0 | 旧文档曾记录量产主路径使用虚拟电流循环；当前源码已复核为 `DataLoad_Current()` | `DataDeal.c:1063-1085` | 当前不再按 bug 处理，但必须防止测试入口回流量产 |
| BUG-002 | P1 | 均衡开压参数被硬编码 4160 覆盖 | `SH367309_DataDeal.c:58-59` | 需确认是固定需求还是 bug |
| BUG-003 | P1 | CAN 版本字段固定 1 | `CanFeidaoFrames.c:158-166` | 可能不符合上位机/客户诊断 |
| BUG-004 | P1 | LedBar fault 分支为空 | `LedBar.c:1022-1024` | 可能未完成故障显示 |
| BUG-005 | P1 | ENV2/ENV3 强制 -40 | `DataDeal.c:227-277` | 可能是无传感器占位，也可能误导 |
| BUG-006 | P1 | Keil 地址口径和安全规则需要统一验证 | `uvprojx` 与 `Flash.h`/脚本 | 风险高，但需以最终 map 复核 |

## 12. 需要硬件实测确认的问题

1. 真实 AFE CADC 电流路径、零点、充放电方向、死区和 SOC 联动。
2. 实际 Flash 容量、后 64K 写入可靠性、掉电中断恢复。
3. IAP 入口、App 起始地址、错误烧录保护。
4. RTC STOP 电流、10s IWDG 周期唤醒功耗、CAN wake service 是否必要。
5. CHG_IN、MCU_WK、SW、RS485/UART wake 源的电平和误唤醒。
6. LedBar 查理复用扫描亮度、鬼影、STOP 前 GPIO 泄漏。
7. 老化模式 MOS 行为、停止/完成/复位持久化。

## 13. 需要确认的业务需求

优先看 `docs/review/requirement_questions.md` 的 `Q-CRIT-001` 到 `Q-CRIT-010`。其中前五项直接决定是否可以进入代码重构。

## 14. 建议下一步

1. 先按 `requirement_questions.md` 逐条确认 P0/P1 需求。
2. 同时把文档体系收敛到 `docs/README.md` 指向的权威文档。
3. 在不改源码的前提下，先补协议地址表、Flash 地址门禁说明、低功耗测试矩阵。
4. 当前状态变量净删减专项见 `docs/review/state_variable_audit.md`；第一批建议从 `ProductionID.c` 一次性 flag 或 LedBar 显式初始化这种低风险项开始。
5. `readyToSleep` 已按 sleep commit 顺序收口为 `rtc_sleep()` 局部 `sleep_mode` 决策；debug/ST-Link ready 改为派生观察值。

## 15. 状态变量净删减专项补充（2026-06-02）

状态：部分验证

当前项目确实存在一类“因为入口和阶段没收口而产生的状态变量”，但不能把所有状态变量都视为复杂度来源。按当前源码审计：

| 类型 | 代表变量 | 判断 |
|---|---|---|
| 已处理收口候选 | `s_ledbar.initialized` 主循环懒初始化、`g_stLowPowerRtcStatus.readyToSleep`, `ProductionID.c` 的 `su8_StartUpFlag` | 已分批净删减；剩余项仍需按状态变量专项文档继续确认 |
| 需要保留的真实历史状态 | 按键防抖、`MCU_WK` 防抖、`scan_index`、`s_u32LastAfeCurrentSampleSeq`、AFE fault 计数、RTC elapsed | 不能因为主循环时序固定而删除 |
| 需求不清的客户逻辑状态 | `DataDeal.c` 中充电器插拔、MOS 过温、UL 认证、RF_EN 熔断类状态 | 必须先确认客户/认证需求归属 |
| debug/status mirror | `g_stLowPowerRtcStatus` 中 `rtcWake/delay/elapsed` 等展示字段 | 可考虑从控制结构迁移到 debug 快照 |

后续源码修改必须先确认 `REQ-SV-*` 或 `Q-SV-*` 条目。
