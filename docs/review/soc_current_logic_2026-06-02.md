# SOC 当前逻辑与校准策略梳理

文档状态：历史归档，已合并
归档日期：2026-06-03
当前权威入口：`docs/design/soc_design.md`

## 归档说明

本文件原用于 2026-06-02 SOC 当前逻辑梳理。2026-06-03 已完成 SOC 模块源码复审和文档合并，当前源码事实统一维护在：

- `docs/design/soc_design.md`

为避免旧结论与当前源码冲突，本文件不再维护完整 SOC 行为描述。

## 已被新文档覆盖的关键修正

| 旧文档口径 | 当前源码事实 |
|---|---|
| 普通 `RELAX` 自耗不直接扣，RTC 休眠补偿扣自耗 | 普通运行 `RELAX` 已按 `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA` 积分；RTC STOP 补偿不再额外扣自耗 |
| RTC 补偿包含自耗和静置下修 | RTC 补偿当前只推进静置 OCV 计数和长静置下修 |
| mid-tail 使用旧 `500/600/650/700mV` 慢 tick 表 | mid-tail 表、计数、debug 字段和测试模型已删除；当前只保留 low-tail 表 |
| 可观察 `SOC_WATCH_BLOCK_REASON/u8LastBlockReason` | 阻塞原因枚举和字段已删除，调试以 tail active、last calib source、内部/显示 SOC 为主 |
| `SOC_IntEnhance_Ctrl()` 拆出 `soc_run_cycle_calibration()`/`soc_update_rest_after_cycle()` | 当前主流程已改为在 `SOC_IntEnhance_Ctrl()` 内直线展开，减少 helper |

## 当前应阅读的文档

| 目的 | 文档 |
|---|---|
| SOC 当前设计、源码 review、校准策略、风险和测试入口 | `docs/design/soc_design.md` |
| 无放电静置快降专题排查 | `docs/review/soc_rest_fast_drop_analysis_2026-06-03.md` |
| SOC 简化执行记录 | `docs/review/soc_simplification_candidates_2026-06-02.md` |
