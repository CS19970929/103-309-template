# 103-309 BMS 后续分阶段重构计划

> 状态：计划文档。
> 前提：未完成需求确认前，不进入源码重构。所有阶段都必须可回滚、可验证、保护上位机协议、安全逻辑和硬件行为。

## 轻量重构原则

后续整体架构优化必须优先保持代码清晰、简单、直接，方便人工阅读和上板排查。允许的优先动作是删除无用代码、删除重复代码、删除不必要变量、减少裸全局变量、简化数据流和控制流；模块边界要清楚，但分层必须轻量。

禁止为了“架构感”引入复杂框架、多层 wrapper、深层嵌套或大而全抽象。任何简化都不能影响功能、协议兼容、硬件行为、SOC、保护、低功耗和 IWDG；涉及这些边界时，必须先拆成小批次并补验证。

## 阶段 1：需求确认阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 只改 `docs/review/*` 和权威文档，不改源码 |
| 不能改什么 | 不改 `.c/.h`、Keil 工程、编译宏、协议行为 |
| 验证方法 | 确认 `requirement_questions.md` 中 P0/P1 项逐条有用户决定 |
| 需要确认 | 电流路径、均衡、Flash 容量/IAP 地址、Host 写权限、老化、低功耗策略 |
| 回滚方式 | 删除或恢复新增文档即可 |

## 阶段 2：文档补全阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 建立 `docs/README.md`, `docs/project_overview.md`, `docs/architecture.md`, `docs/module_map.md`, `docs/design/*`, `docs/test/*` |
| 不能改什么 | 不移动/删除旧文档，除非后续确认 |
| 验证方法 | 每份权威文档开头标注源码验证状态、参考源码、未确认事项 |
| 需要确认 | 哪些历史文档可以归档，哪些客户需求仍有效 |
| 回滚方式 | 保留旧文档，删除新权威文档即可回到原状态 |

## 阶段 3：模块边界整理阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 先只新增接口设计文档；代码阶段只允许小范围拆分 `DataDeal.c` 的只读 helper 或 wrapper |
| 不能改什么 | 不改采样周期、不改 `g_stCellInfoReport` 字段布局、不改协议地址 |
| 验证方法 | 编译、Modbus `0xD000/0xC002/0xD300` 回归、CAN 周期帧抓包 |
| 需要确认 | 是否允许把客户逻辑从 `DataDeal.c` 拆到独立模块 |
| 回滚方式 | 每个 wrapper 单独 commit；不改变行为时可直接撤回该 commit |

## 阶段 4：Flash / EEPROM / 参数存储阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 固化 Flash 布局文档、增加地址/容量检查脚本、明确 EEPROM 兼容层命名 |
| 不能改什么 | 不改存储地址、不改 record 格式、不改写入策略，除非确认硬件容量 |
| 验证方法 | 后 64K 快速测试、掉电恢复测试、参数写失败回滚测试、map 地址检查 |
| 需要确认 | 真实 Flash 容量、工厂是否允许后 64K 存储、升级参数策略 |
| 回滚方式 | 脚本和文档可单独回滚；存储格式变更必须另立迁移阶段 |

## 阶段 5：通信协议适配层阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 先生成权威 Modbus/CAN 协议表；后续代码只抽读写副作用表 |
| 不能改什么 | 不改地址、字段顺序、异常码、CAN ID、`0xC002`、`0x14F80208` |
| 验证方法 | 上位机实时监控、产品信息读取、老化时间读取、CAN App read/write/block read |
| 需要确认 | Host 写权限、CAN service 命令集、老化控制是否保留 |
| 回滚方式 | 协议层改动必须小 commit，保留旧函数入口 |

## 阶段 6：ADC / AFE 数据流阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 恢复/隔离真实电流路径、梳理 AFE sample -> DataLoad -> SOC 顺序 |
| 不能改什么 | 不改 AFE 保护阈值、不改 MOS 初始策略、不改 I2C 时序 |
| 验证方法 | AFE 电流方向/零点实测、SOC sample seq、CAN 电流、Modbus 电流一致性 |
| 需要确认 | `test_Autocurrent_cycle()` 归属、Type-C 电流是否计入 SOC |
| 回滚方式 | 独立 feature guard；问题时可切回旧调用路径 |

## 阶段 7：SOC 阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 先补测试，再整理状态命名/输入输出边界 |
| 不能改什么 | 不改 1% 校准硬约束、不改满电/低压锚点、不改 `0xD300` 隔离 |
| 验证方法 | 主机回放、真实充放电、RTC rest、snapshot 断电恢复、上位机读取 |
| 需要确认 | 初始 SOC、OCV 表、Type-C 电流、测试模式策略 |
| 回滚方式 | 算法参数与结构调整分离，先保留旧字段 |

## 阶段 8：RTC 低功耗 / IWDG 阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 先统一低功耗需求、当前实现、阻塞原因、RTC 周期策略、IWDG/DBGMCU/AFE/老化边界，再做小批次净删减 |
| 不能改什么 | 未确认前不改唤醒源电平、不改协议、不改 IAP/Flash busy 阻塞、不改 AFE sleep 行为、不改量产/测试隔离 |
| 验证方法 | STOP 电流、唤醒源、通信恢复、IWDG 长稳、过放深睡、老化运行、AFE fault、参数写入后策略变化 |
| 需要确认 | 工厂老化是否阻塞 STOP、AFE not idle 是否阻塞、HICCUP 前 AFE 是否 sleep、Sleep 参数哪些对外有效；IWDG 已按量产稳定优先统一，但功耗取舍仍需实测 |
| 回滚方式 | 每个净删减批次单独提交；保留旧 `rtc_sleep()` 执行器直到上板验证完成，失败可回退单个批次 |

### 阶段 8 推荐小批次

| 批次 | 类型 | 内容 | 验证 |
|---|---|---|---|
| LP-01 | 文档确认 | 以 `docs/review/low_power_requirement_alignment_2026-06-02.md` 为主表，逐条确认低功耗需求 | 用户确认需求表，源码零改动 |
| LP-02 | 删除未使用入口 | 已删除未被主路径调用的 `LP_EnterStop()`、`LP_BeforeSleep()`、`LP_AfterWakeup()`、`LP_SetWakeupPeriod()` wrapper、无消费者 `LP_State_t` 缓存，以及独立 `app_lowpower.c/h` 模块 | `rg` 确认源码无外部调用，低功耗主路径不变；仍需 Keil 编译 |
| LP-03 | 统一阻塞原因 | 让 `LOW_POWER_RTC_BLOCK_*` 与 `LP_BLOCK_*` 保持一致，删除不会产生的 reason 或补齐真实触发点 | 主机静态检查，通信/Flash/LED/fault 阻塞回归 |
| LP-04 | DBGMCU Release 门控 | 已删除无条件 `__EnableLowPowerDebug__`，Release 默认清除低功耗调试位，Debug/定位时显式打开 DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP | ST-Link 读 `DBGMCU->CR`，STOP 电流对比 |
| LP-05 | IWDG 策略统一 | 已统一 `PROJECT_CFG_WDOG_ENABLE`、`Init_IWDG()`、`IWDG_Feed()` 和 RTC wake 安全窗口含义 | `PROJECT_CFG_WDOG_ENABLE=0/1` 分别构建和长稳测试 |
| LP-06 | 老化/AFE 阻塞确认后实现 | 按确认结果处理 FactoryAging 和 AFE not idle 的 STOP 阻塞或文档化不阻塞 | 老化 running、AFE fault/PCHG/MOS 状态下实测 |

## 阶段 9：LED / 显示阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 明确显示模型：SOC、图标、故障、睡眠预览、按键 |
| 不能改什么 | 不改 GPIO 引脚、不改 TIM4 扫描频率、不改低功耗前引脚安全态 |
| 验证方法 | 各 SOC 值显示、按键显示窗口、长按休眠、STOP 前泄漏、故障显示 |
| 需要确认 | 充电图标语义、故障显示模式、长按时间 |
| 回滚方式 | 显示模型独立 commit，可回退到当前 `APP_LedBar()` |

## 阶段 10：最终回归测试阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 只做验证、文档更新、发布检查 |
| 不能改什么 | 不在回归阶段混入功能变更 |
| 验证方法 | 编译、Modbus、CAN、存储、SOC、ADC/AFE、保护、MOS、低功耗、IAP、LED、老化 |
| 需要确认 | 出货标准、客户协议版本、硬件测试清单 |
| 回滚方式 | 每阶段都有独立 commit 和文档记录，失败回滚到上一个已验证阶段 |

## 阶段 11：状态变量净删减专项阶段

专项文档：`docs/review/state_variable_audit.md`

| 项目 | 内容 |
|---|---|
| 修改范围 | 只处理当前源码中“重复事实、阶段残留、一次性初始化 flag、纯 debug mirror”等状态变量 |
| 不能改什么 | 未确认前不改保护阈值、SOC 算法、CAN/Modbus 协议、IAP/App 地址、Flash 布局、AFE sleep/唤醒时序、客户认证逻辑 |
| 验证方法 | 每批先 `rg` 确认调用链，再 `git diff --check`；涉及源码时执行仓库脚本、能用的静态检查/编译、对应模块实测 |
| 需要确认 | `Q-SV-001` 到 `Q-SV-006`，尤其是 LedBar 显式初始化、`readyToSleep` 收口、DataDeal 客户逻辑归属 |
| 回滚方式 | 每个候选独立小 commit；出现 LED/低功耗/日志异常时只回滚该批 |

### 阶段 11 推荐小批次

| 批次 | 类型 | 内容 | 允许修改文件 | 前置条件 | 验证 |
|---|---|---|---|---|---|
| SV-00 | 文档确认 | 维护 `state_variable_audit.md`、需求确认表、风险和测试计划 | `docs/review/*`, `docs/change_log.md`, `docs/test_plan.md` | 本阶段已开始 | 文档链接和源码证据自洽 |
| SV-01 | 已完成低风险试点 | 把产品信息初始化从 `ProductionID.c` 的 `su8_StartUpFlag` 收口到启动流程，并保留 PROID heartbeat hook | `AppInit.c`, `ProductionID.c/.h`, 文档 | 用户确认“先只做低风险” | `0xC002` 默认信息读取、编译、`rg su8_StartUpFlag` |
| SV-02 | 初始化收口 | 显式调用 `LedBar_Init()`，减少分散 `LedBar_EnsureInit()` | `AppInit.c`, `LedBar.c/.h`, 文档 | 用户确认 `Q-SV-001`；确认 TIM4 ISR 使能顺序 | LED 启动显示、按键显示、TIM4 扫描、STOP 前 GPIO、低功耗释放 |
| SV-03 | 低功耗提交收口 | 将 `readyToSleep` 改成本地提交决策或明确 commit pending，收口 LED/日志 sleep 前动作 | `rtc_sleep.c/.h`, `LedBar.c`, `LogRecord.c`, `Runtime.c`, 文档 | 用户确认 `Q-SV-002`；画出 sleep commit 顺序 | HICCUP/NORMAL/DEEP、BMS_SLEEP 日志、sleep SOC、`SystemDebug` 低功耗快照 |
| SV-04 | debug mirror 收口 | 从控制结构里移出纯展示字段，或标记为 debug mirror | `rtc_sleep.c/.h`, `SystemDebug.c/.h`, 文档 | 用户确认 `Q-SV-003`；确认工具/上位机依赖 | `SystemDebug`、ST-Link 监控、Modbus debug 窗口 |
| SV-05 | DataDeal 需求拆分 | 先只把 `charger_detect_and_keyLogi_200ms()`、`new_todo_logi()` 中状态需求归类，不先删除 | 文档优先，源码另批 | 用户确认 `Q-SV-006` | 需求表通过后再决定是否拆模块 |

### 阶段 11 明确保留项

以下变量不是第一批净删减目标：

- 按键和 `MCU_WK` 防抖/边沿状态。
- `scan_index`、LED frame 和 TIM4 扫描状态。
- SOC 的 `s_u32LastAfeCurrentSampleSeq`。
- AFE fault/recover 计数和低压/强制低压累计计数。
- CAN/Sci RX/TX 队列、pending、busy、read block 状态。
- Flash busy、日志边沿去重、BKP sleep flag 和 RTC elapsed 秒数。
