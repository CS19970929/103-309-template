# SOC 源码简化执行记录

文档状态：历史参考，已合并至 `docs/design/soc_design.md`
源码验证日期：2026-06-04
当前权威入口：`docs/design/soc_design.md`

归档说明：本文保留 SOC 简化执行脉络；当前源码事实、已删除项、风险与测试入口统一维护在 `docs/design/soc_design.md`。

## 1. 本轮目标

本轮目标是按用户要求简化 SOC 模块写法，删除无用代码和误导性状态，不增加过多 helper，不改 low-tail 表，不改协议字段和硬件时序。

硬边界：

- 不改 `s_empty_tail_table` 的活动值和结构。
- 不改满电、低压 low-tail、显示平滑、Type-C 折算、协议字段和 Flash 地址。
- 不引入 HAL、RTOS、malloc 或新框架。
- 保留当前 200ms AFE sample seq 驱动的调度顺序。

## 2. 已执行源码简化

| 项目 | 文件 | 结果 |
|---|---|---|
| 恢复 low-tail 主流程 | `SocEnhance.c` | 保留 low-tail 计算和满/空校准主流程 |
| 删除 mid-tail | `SocEnhance.c/.h`, `SystemDebug.c/h`, `tools/soc_replay_test.py` | 删除 mid-tail 表、计数、debug 字段和测试模型 |
| 减少 helper | `SocEnhance.c` | 删除 `soc_run_cycle_calibration()` 和 `soc_update_rest_after_cycle()`，将核心阶段直线展开在 `SOC_IntEnhance_Ctrl()` |
| 删除 deferred OCV 自动路径 | `SocEnhance.c` | 删除短静置锁存和 active 放电消化，只保留长静置慢速下修 |
| 去掉 RTC 自耗扣减 | `SocEnhance.c` | 删除 `soc_apply_board_self_consumption_seconds()`；RTC STOP 只推进静置/OCV 补偿 |
| 删除无用配置字段 | `SocEnhance.h`, `SOC.c` | 删除 `u16_SOC_CycleT_Limit` 及唯一赋值 |
| 删除无用 OCV 标志 | `SocEnhance.h`, `SystemDebug.c/h` | 删除 `u8_SOC_OCV_Cali` 及 debug 快照/打印 |
| 删除 block reason | `SocEnhance.h`, `SocEnhance.c` | 删除 `SOC_WATCH_BLOCK_REASON`、`u8LastBlockReason`、`soc_watch_set_block_reason()` 和调用点 |
| 精简 debug monitor | `SystemDebug.c/h`, `SocEnhance.h/c` | 删除固定 0 或伪造派生字段，只保留真实内部计数和 `display_ticks` |
| 删除死代码 | `SOC.c` | 删除 `#if 0` 中空的 `SOC_TestMode_RunSample()` 和 `SOC_TestMode_ReadStatus()` |
| 补齐 host stub | `tools/soc_host_c_test.c`, `tools/soc_host_visual_trace.c` | 补 `ADC_GetVbatMilliVolt()`、`AfeCurrent_GetSeq()`，host C 测试和 visual trace 可按当前接口链接 |
| 更新 replay 表解析 | `tools/soc_replay_test.py` | 解析活动 C 源码和 `DELAY_SOC_TEST` 宏，避免误读 `#if 0` 对照表 |

## 3. 当前 `SOC_IntEnhance_Ctrl()` 结构

当前主函数保留一条直线，方便 review：

```text
命令处理
  direction
  integrate
  sag hold
  low-tail config
  full/empty
  rest timer or reset rest confidence
  save if needed
  publish
```

这个顺序是 SOC 行为边界，后续重构不能随意调整。

## 4. 自耗口径确认

当前源码事实：

- 正常运行 `RELAX` 已经按 `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA` 积分。
- `CHG` 会扣掉板载自耗后再充入容量。
- `DSG` 会把板载自耗叠加到放电电流。
- RTC STOP 补偿不再额外扣自耗。

该口径已由 host C 测试覆盖：

- `test_board_self_consumption_integrates_during_relax`
- `test_rtc_sleep_does_not_apply_board_self_consumption`
- `SOC_TEST_BOARD_SELF_CONSUMPTION_MA=0/30/1000` 覆盖组合

## 5. Tail 表状态

当前保持：

- 活动 `s_empty_tail_table` 保留。
- 当前活动 tick 全部来自 `DELAY_SOC_TEST = 5`。

本文只记录当前源码事实；mid-tail 已删除，low-tail 表仍按用户测试状态保留。

## 6. 验证结果

已执行：

- `python3 tools/soc_replay_test.py`：43 项通过。
- `python3 tools/run_soc_host_c_test.py`：`30mA/0mA/1000mA` 和 debug-watch 组合均通过。
- `python3 tools/soc_visual_report.py --html build/host_tests/soc_visual_report_check.html --csv build/host_tests/soc_visual_trace_check.csv`：5 个场景通过。
- `git diff --check`：通过。
- `python3 tools/project_check.py --quiet`：当前 checkout 为历史基线失败 `99 OK / 1 warning / 39 errors`，未见本轮 SOC/ADC/DataDeal 门禁新增失败。

仍需执行：

- Keil `FD_Release`
- 真板充放电、RTC STOP、CAN/Modbus、Keil watch

## 7. 暂不建议继续简化的点

| 项目 | 原因 |
|---|---|
| 拆更多 helper | 当前主流程已足够短，继续拆会增加跳转成本 |
| 移动 low-tail 表或改 tail 值 | 用户正在测试 tail，本轮冻结 |
| 把 `SOC_Enhance_Element` 大幅私有化 | 会影响 Keil watch 和现有协议调试习惯，需要单独确认 |
| 改 `display_soc` 策略 | 用户可见体验变化，需要上板验证 |
| 给 reset sleep 增加 RTC 秒数 SOC 补偿 | 已确认不需要；不要实现 |
| RELAX 下继续调整 low-tail | 功能体验变化，需先用 watch 确认快降来源 |
