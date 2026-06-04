# SOC 函数粒度审查与净删减记录

文档状态：已按源码验证
源码验证日期：2026-06-04
当前权威入口：`docs/design/soc_design.md`
主要参考源码：`103 + 309/Project/Source/SocEnhance.c`

## 1. 审查原则

本轮按“嵌入式可读性优先”的标准 review SOC 模块函数粒度。

判断标准：

- 保留对应真实边界的函数：Flash snapshot、RTC 补偿、OCV 表查找、low-tail 业务规则、Debug Watch 刷新。
- 保留主状态机线性顺序：命令、方向、积分、sag hold、low-tail/full、rest、保存、发布。
- 删除只转发一层、只用一次且不能降低理解成本的 helper。
- 删除历史泛化抽象：mid-tail、display_soc 删除后，不继续保留为未来假设服务的通用查表或显示包装。

## 2. 本轮处理

| 项目 | 原实现 | 当前处理 | 原因 |
|---|---|---|---|
| 发布包装层 | `soc_publish()` 只转发到 `soc_export_public_fields()` | 合并为单个 `soc_publish()` | 删除 `display_soc` 后不再需要两层发布函数 |
| 积分模式判断 | `soc_integrate_mode_from_current()` 只被 `soc_integrate()` 调用 | 合并回 `soc_integrate()` | 正负电流转充/放/静置是积分本地逻辑 |
| 长静置 OCV 单步 | `soc_apply_ocv_target_step(target, mode)` 只剩 long-rest 调用 | 合并回 `soc_apply_long_rest_down_step()` | 手动 OCV/deferred OCV 已删除，泛化 `mode` 参数不再有价值 |
| 重放电判断 | `soc_heavy_discharge_active()` 只被 `soc_update_sag_hold()` 调用 | 合并回 `soc_update_sag_hold()` | 条件短且只服务 sag hold |
| low-tail 泛化查表 | `soc_tail_rule_lookup()` / `soc_empty_tail_config()` / `soc_empty_current_band()` 多层组合 | 收敛到 `soc_low_tail_config()` | mid-tail 已删除，只剩一个 low-tail 表，保留泛化参数会增加跳转 |
| 空电偏移判断 | `soc_vmin_above_empty_offset()` 只服务 low-tail 包装层 | 合并到 `soc_low_tail_config()` | 只用一次，且 threshold 语义在 low-tail 里更直观 |
| 缩进噪声 | snapshot load/save 处有缩进残留 | 修正缩进 | 降低 review 噪声，不改变逻辑 |
| 积分电流转 signed mA | `soc_integrate_current_ma()` 只被 `soc_integrate()` 调用 | 合并回 `soc_integrate()` | CHG/DSG/RELAX 电流口径是积分本地逻辑，减少跳转 |
| 空电/满电默认值 helper | `soc_empty_mv()`、`soc_full_mv()` 都只被一个函数调用 | 合并到 `soc_empty_threshold_mv()` 和 `soc_full_confirm_seconds()` | 只做默认值选择，独立函数不形成业务边界 |
| 保存判重包装 | `soc_save_mark_changed()` 只服务 `soc_save_if_needed()` | 合并回 `soc_save_if_needed()` | 判重字段在唯一调用点直接可见，Flash 写入边界仍由 `soc_save_current_snapshot()` 保留 |
| tail disabled 分支 | `SOC_TAIL_TARGET_DISABLED` 当前没有任何活动表项使用 | 删除宏和判断 | 当前源码没有禁用表项，保留 0xFF 预留分支会误导 review |

## 3. 明确保留

| 函数/边界 | 保留原因 |
|---|---|
| `SOC_IntEnhance_Ctrl()` | SOC 主状态机入口，必须保持线性可读 |
| `soc_integrate()` | 安时积分和自耗处理边界 |
| `soc_apply_full_empty()` | 满电锚点和 low-tail 校准边界 |
| `soc_low_tail_config()` | low-tail 表选择和电流 band 规则边界 |
| `soc_update_rest_timer()` / `soc_apply_rtc_rest_ocv()` | 正常 200ms 静置与 RTC STOP 秒级补偿是两个调度来源 |
| `soc_save_current_snapshot()` / `soc_save_if_needed()` | Flash 写入与保存判重边界 |
| `soc_watch_refresh()` | Debug Watch 观测边界 |

## 4. 行为边界

本轮未修改：

- low-tail 表值、匹配顺序和 `DELAY_SOC_TEST`。
- 满电确认阈值、静置 OCV 阈值、RTC 补偿阈值。
- 200ms AFE sample seq 调度顺序。
- Flash snapshot 结构和保存字段。
- Modbus/CAN/LedBar 发布字段。
- `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT = 1` 的单步校准约束。

## 5. 后续建议

后续继续重构 SOC 时，优先问两个问题：

1. 这个函数是不是一个真实业务边界？
2. 这个函数是否被多个地方复用，或能显著降低调用点复杂度？

如果两个答案都是否，优先合并回调用点，而不是继续保留包装层。
