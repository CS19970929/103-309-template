# SOC 源码简化候选

文档状态：已按源码部分验证
源码验证日期：2026-06-02
修改状态：只读分析，未修改源码
主要参考源码：`103 + 309/Project/Source/SOC.c`、`SocEnhance.c`、`SocEnhance.h`、`DataDeal.c`、`Sci_Upper.c`、`rtc_sleep.c`、`rtc_sleep_port.c`、`LowPowerSleep.c`、`LedBar.c`、`Flash.c`、`conf/Project_Config.h`
目标：只优化软件实现和写法，方便阅读、维护和 Keil watch 调试；不改变功能、协议、时间参数、校准阈值、休眠顺序和用户可见行为。

## 1. 总原则

SOC 模块可以继续简化，但必须从低风险、小步、可回滚开始。

允许优先处理：

1. 重复发布、重复初始化、一次性命令 shadow。
2. 公共结构体里混杂的配置、输入、输出、命令字段。
3. 调试 watch 可见性和命名不清问题。
4. 只影响阅读路径、不影响计算顺序的局部整理。

暂不建议处理：

1. SOC 表、满电/低压/中段/静置/RTC 的阈值和时间参数。
2. `display_soc` 平滑策略。
3. `HICCUP_MODE` RTC STOP 时序和 `NORMAL/DEEP` reset sleep 时序。
4. Modbus/CAN 对外字段。
5. Type-C 电流是否计入 SOC 的产品语义。

## 2. 低风险候选

### SOC-SIM-01：删除初始化阶段重复发布

当前事实：

- `soc_param_lib_init()` 内部已经调用 `soc_publish(1U)`。
- `InitData_SOC()` 随后又调用 `SOC_PublishReportData()`。

候选动作：

- 保留 `soc_param_lib_init()` 内部发布。
- 删除 `InitData_SOC()` 里的重复 `SOC_PublishReportData()`。

保持不变：

- 启动 SOC 初值、display_soc、容量字段、Modbus/CAN 发布口径不变。

风险：

- 低。需要确认 `soc_param_lib_init()` 所有返回路径都已经发布；当前源码看是成立的。

验证：

- `clang -fsyntax-only SOC.c SocEnhance.c`
- `tools/soc_replay_test.py`
- 上板读取 `0xD000` SOC/容量字段。

### SOC-SIM-02：为 SOC 命令增加请求接口，减少外部直接写 public shadow

当前事实：

- `SOC_Enhance_Element.u16_RefreshData_Flag` 和 `u8_SetSocOnce` 是公共结构体字段。
- `Sci_Upper.c` 直接写 flag `2/3` 和 set-once 值。
- `soc_handle_command()` 再消费这些字段。

候选动作：

- 增加 `SOC_RequestManualOcvRefresh()`、`SOC_RequestCapacityReset()`、`SOC_RequestSetOnce(UINT8 soc)`。
- `Sci_Upper.c` 改为调用请求接口。
- 第一步可保留原字段布局，不立刻删除字段，降低协议和 debug 风险。
- 第二步再评估能否把命令字段从 public struct 移入私有 runtime。

保持不变：

- `flag 1/2/3` 对应行为不变。
- `0x06 SetSocOnce` 合法范围和 ACK/NEG 不变。
- 容量参数写入后的 SOC 重算行为不变。

风险：

- 低到中。触及 `Sci_Upper.c` 和 `SocEnhance.h`，但不改算法。

验证：

- `rg "u16_RefreshData_Flag|u8_SetSocOnce"` 确认消费者减少。
- Modbus 写一次 SOC、写容量参数、手动 OCV refresh 回归。

### SOC-SIM-03：拆清 `SOC_Enhance_Element` 的字段角色文档和 watch 口径

当前事实：

- `SOC_Enhance_Element` 同时包含配置、采样输入、输出、命令字段。
- 这方便 Keil watch，但阅读时容易误以为所有字段都对外发布或都能被协议写。

候选动作：

- 先只在 `SocEnhance.h` 注释中分组：config/input/output/command。
- 若后续允许，再把私有命令和输入字段迁入 `SOC_STATE` 或独立 `SOC_RUNTIME`，同时保留 watch 指针。

保持不变：

- 结构体布局先不动，避免影响 watch、协议工具和调试习惯。

风险：

- 低。第一步只改注释；第二步要另立批次。

验证：

- 注释阶段只需 `git diff --check`。
- 若移动字段，需 SOC 回放、Modbus/CAN、Keil watch 回归。

### SOC-SIM-04：统一 SOC 时间参数的“秒”和“tick”命名

当前事实：

- `SOC_REST_OCV_SECONDS`、`SOC_SHORT_REST_MIN_SECONDS` 是秒。
- `rest_ticks/stable_rest_ticks/short_rest_ticks` 是 200ms tick。
- RTC 休眠路径用 seconds 转 tick，普通路径每 200ms 自增。

候选动作：

- 仅调整局部变量名和注释，明确 `ticks` 是 SOC tick，不是 RTC tick。
- 不改任何宏值和换算公式。

保持不变：

- 所有时间阈值、计数行为、RTC 秒数补偿不变。

风险：

- 低。主要是可读性提升。

验证：

- `git diff --check`
- `tools/soc_replay_test.py`

### SOC-SIM-05：把低压尾端和中段尾端的表格说明贴近源码

当前事实：

- `s_empty_tail_table`、`s_mid_tail_table` 是 SOC 体验核心，但代码里只体现数字表。
- 目标和 ticks 的业务含义需要靠文档反推。

候选动作：

- 在表格前补极短注释，说明 offset/targer/ticks/band。
- 不拆 helper，不改表值。

保持不变：

- 表值、查表顺序、target/ticks 语义不变。

风险：

- 低。

验证：

- `git diff --check`

## 3. 中风险候选

### SOC-SIM-06：整理 `soc_publish()` 的职责

当前事实：

- `soc_publish()` 同时做显示目标选择、display_soc 平滑、输出字段更新、debug watch 刷新和对外发布。

候选动作：

- 可拆成最多 2 个内部静态函数：
  - `soc_update_display_soc(force_display)`
  - `soc_export_public_fields(force_display)`
- 不拆出新文件，不改变发布顺序。

保持不变：

- `force_display` 行为不变。
- fixed/zero 覆盖行为不变。
- 显示平滑秒数和低压加速行为不变。

风险：

- 中。该函数影响用户显示、CAN、Modbus 和 debug watch；必须有回放和上板确认。

### SOC-SIM-07：整理 `SOC_IntEnhance_Ctrl()` 主流程注释和局部变量

当前事实：

- 主流程已经集中，但 `calibrated/low_tail_active/mid_tail_active` 阅读成本高。
- 当前顺序很重要：积分 -> sag hold -> 低压/中段 -> 满空 -> deferred -> rest timer -> save -> publish。

候选动作：

- 只补短注释或重命名局部变量，不改变函数拆分和调用顺序。
- 不把每一步拆成一堆 wrapper。

保持不变：

- 调用顺序完全不变。

风险：

- 中低。只要不重排顺序，风险可控。

### SOC-SIM-08：收口 RTC 休眠补偿的状态说明

当前事实：

- `s_u32RtcRestCursorSeconds` 是 SOC 内部 RTC rest cursor。
- `s_u32RtcSleepElapsedSeconds` 是低功耗模块累计休眠秒数。
- 两者分属不同模块，名字接近但职责不同。

候选动作：

- 补注释或改更明确的私有变量名。
- 不改接口 `SOC_ApplyRtcRelaxationCompensation(rest_seconds, vmin, vmax)`。

保持不变：

- HICCUP STOP 补偿时序不变。
- reset sleep 不新增补偿，除非后续单独确认。

风险：

- 中低。涉及低功耗理解，但不改行为。

## 4. 暂不建议做的候选

| 候选 | 暂缓原因 |
|---|---|
| 合并 `SOC.c` 和 `SocEnhance.c` | 可能减少文件数量，但会把配置装载、Type-C 折算、核心算法混在一起；当前收益不如先清 command/public struct |
| 拆成 `soc_core/soc_display/soc_ocv/soc_storage` 多文件 | 长期边界清晰，但当前会增加跳转和构建风险；不符合“先简化写法”的目标 |
| 修改静置 OCV 上修/下修策略 | 会改变用户体验和 SOC 准确性，不属于“功能不动” |
| 修改 RTC reset sleep 后 SOC 补偿 | 是功能行为变更，需要先确认休眠秒数来源和显示体验 |
| 删除 Type-C 等效电流 | 产品语义未确认，不能作为写法优化处理 |
| 删除 low/mid tail 表 | 会改变低端安全和骑行体验，不能做 |

## 5. 推荐执行顺序

建议后续如果进入源码阶段，按以下顺序小步提交：

| 顺序 | 批次 | 风险 | 是否建议先做 |
|---:|---|---|---|
| 1 | `SOC-SIM-01` 删除初始化重复发布 | 低 | 是 |
| 2 | `SOC-SIM-05` 表格短注释 | 低 | 是 |
| 3 | `SOC-SIM-04` 秒/tick 命名和注释 | 低 | 是 |
| 4 | `SOC-SIM-02` SOC 命令请求接口第一步 | 低到中 | 是 |
| 5 | `SOC-SIM-07` 主流程局部整理 | 中低 | 视前四步结果 |
| 6 | `SOC-SIM-06` `soc_publish()` 职责整理 | 中 | 暂缓到回放测试稳定后 |
| 7 | `SOC-SIM-03` public struct 字段迁移 | 中 | 只在 watch/协议依赖确认后做 |

## 6. 源码阶段最低验证

每个源码简化批次至少执行：

1. `git diff --check`
2. `rg` 确认旧符号/调用点符合预期。
3. `clang -fsyntax-only` 检查涉及的 SOC 源文件。
4. `python3 tools/soc_replay_test.py`
5. `python3 tools/project_check.py --quiet`，并和当前基线区分。

若改到发布口、命令口或休眠补偿，还需要：

1. Modbus 读取 `0xD000` SOC/容量字段。
2. Modbus `0x06 SetSocOnce`。
3. CAN SOC 周期帧。
4. RTC HICCUP STOP 唤醒后 SOC/显示一致性。
5. Keil watch 查看 `s_soc`、`SOC_Enhance_Element`、`g_stCellInfoReport.SocElement`。
