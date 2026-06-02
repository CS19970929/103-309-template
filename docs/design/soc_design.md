# SOC 设计入口

文档状态：已按源码验证
源码验证日期：2026-06-02
主要参考源码：`103 + 309/Project/Source/SOC.c`、`SocEnhance.c`、`SocEnhance.h`、`DataDeal.c`、`rtc_sleep.c`、`rtc_sleep_port.c`、`LowPowerSleep.c`、`LedBar.c`、`Sci_Upper.c`、`Flash.c`、`System_Monitor.c`、`conf/Project_Config.h`
未验证事项：未执行 Keil 编译、上板充放电、RTC STOP 功耗、CAN/Modbus 在线读取和 ST-Link watch 实测。

## 1. 当前权威文档

本文件作为 SOC 模块的长期设计入口，避免历史 devlog、旧阶段方案和当前源码事实混在一起。

| 用途 | 文档 |
|---|---|
| 当前源码完整逻辑、所有校准策略、时间参数 | `docs/review/soc_current_logic_2026-06-02.md` |
| 后续只改写法、不改功能的源码简化候选 | `docs/review/soc_simplification_candidates_2026-06-02.md` |
| 顶层测试入口 | `docs/test_plan.md`、`docs/review/test_plan.md` |
| 项目级风险 | `docs/review/risk_list.md` |
| 历史开发记录 | `docs/devlog/*`，仅作追溯，不作为当前事实 |

## 2. 当前源码事实

当前 SOC 是混合规则模型：

1. 主估算以容量积分为主，入口为 `App_AFEGet()` 200ms AFE 采样后调用 `App_SOC()`。
2. `SocEnhance.c` 是核心实现，`SOC.c` 主要负责装载配置、折算 Type-C 电流和调度核心状态机。
3. 内部真实估算值是 `s_soc.soc`，对外显示/通信发布值是 `s_soc.display_soc`。
4. `g_stCellInfoReport.SocElement.u16Soc` 当前发布的是 `display_soc`，因此 CAN、Modbus 和 LedBar 默认看到的是显示 SOC。
5. 自动校准大多按 `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT = 1%` 小步推进。
6. `PROJECT_CFG_BAT_CHEMISTRY = 0` 且 `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE = 0`，当前编译期使用三元锂 OCV 表，上位机运行时 SOC 表写入不参与量产算法。

## 3. 当前校准策略总览

按工程行为归并，当前 SOC 有 11 类策略：

| 类别 | 策略 | 当前作用 |
|---|---|---|
| 1 | 启动 snapshot/default/OCV 初始化 | 决定上电初值 |
| 2 | 充放电容量积分 | 主 SOC 估算 |
| 3 | 板载自耗补偿 | RTC 休眠期间按 `30mA` 扣容量 |
| 4 | 满电锚点 | 满电条件满足后按 `1%` 步进到 100 |
| 5 | 低压尾端下修 | 低端防虚高 |
| 6 | 中段尾端下修 | V0 上方较宽区间防虚高 |
| 7 | 静置 OCV 目标锁存 | 静置稳定后记录下修目标 |
| 8 | 放电期 deferred OCV 下修 | 放电中慢慢消化静置目标 |
| 9 | 长静置下修 | 长时间 relax 后继续按 OCV 目标下修 |
| 10 | RTC 休眠补偿 | HICCUP STOP 周期内自耗+静置下修 |
| 11 | 上位机命令校准 | 手动 OCV、容量重算、一次设置 SOC |

完整条件、时间和源码证据见 `docs/review/soc_current_logic_2026-06-02.md`。

## 4. 用户体验边界

1. 自动校准通常通过 `soc_publish(0)` 发布，会被 `display_soc` 平滑吸收。
2. 初始化、上位机命令和参数重载使用 `soc_publish(1)`，可能立即改变显示 SOC。
3. `HICCUP_MODE` RTC STOP 周期中，SOC 补偿发生在周期唤醒阶段，最终按键唤醒后才请求 LedBar 显示。
4. `NORMAL/DEEP` 复位式休眠当前只保存 snapshot 和 LedBar 快显 SOC，没有看到复位后按休眠秒数调用 RTC SOC 补偿的闭环。

## 5. 后续维护原则

1. 功能不动时，不改 SOC 表、不改校准阈值、不改协议字段含义、不改休眠进入/唤醒顺序。
2. 源码简化优先做命名、状态所有权、重复发布、命令 shadow 收口这类可验证小步。
3. 不为了“架构完整”拆多层文件；只有能减少真实重复、降低调用方复杂度或明确边界时才拆。
4. 每个源码简化批次必须先说明保持不变的行为，再跑 `git diff --check`、可用语法检查、`tools/soc_replay_test.py` 和仓库检查脚本；上板验证不足必须如实标注。
