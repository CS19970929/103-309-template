# SOC 模块等效简化说明

日期：2026-05-12

## 目标

在不改变 SOC 对外行为的前提下，降低 `SocEnhance.c` 内部维护复杂度。外部接口、Flash V2 快照格式、RS485 寄存器地址、SOC 测试模式隔离和量产默认配置均保持不变。

## 本次简化

1. 移除 `SocEnhance.c` 对 `PubFunc.c/GetEndValue()` 的直接依赖。
   - SOC 模块改为使用本地 `soc_table_percent()` 查询 OCV 表。
   - 避免原调用中 `soc_ocv_table(&size)` 与 `size` 同一表达式求值顺序不明确的问题。
   - 主机 C 测试不再需要链接 `PubFunc.c`，避免无关硬件符号影响 SOC 单元验证。

2. 合并低压尾段和中压尾段的重复查表逻辑。
   - 新增统一的 `SOC_TAIL_STEP` 和 `soc_tail_rule_lookup()`。
   - `s_empty_tail_table`、`s_mid_tail_table` 的表数据和阈值含义保持不变。

3. 合并尾段小步校准的计数器推进逻辑。
   - 新增 `soc_apply_tail_step()`。
   - 仍保持每次最多移动 `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT`，当前默认仍为 1%。

4. 合并 SOC 快照保存成功后的状态同步逻辑。
   - 新增 `soc_save_current_snapshot()`。
   - `soc_save_if_needed()`、上位机命令处理、RTC 静置补偿后的保存路径共用同一入口。

5. 主循环只取一次低压/中压尾段配置。
   - `SOC_IntEnhance_Ctrl()` 先生成 `low_tail_step` 和 `mid_tail_step`，后续直接复用。
   - 避免 active 判断和实际应用阶段重复查同一张表。

## 保持不变

- `InitData_SOC()`、`App_SOC()`、`SOC_IntEnhance_Ctrl()`、`SOC_ApplyRtcRelaxationCompensation()` 等外部 API 不变。
- `STORAGE_FLASH_SOC_DATA` V2 字段不变。
- `0xD000`、`0xD300`、`0x1005`、`0x2200`、`0x2318` 等通信地址不变。
- `PROJECT_CFG_BUILD_PROFILE` 默认仍为 `0`。
- `PROJECT_CFG_SOC_TEST_MODE_ENABLE` 默认仍为 `0`，测试模式仍只能通过工厂测试配置开启。
- 安全烧录脚本仍固定 App 地址 `0x08004800`，并保留 dry-run 输出。

## 验证结果

- `py -3.9 tools\soc_replay_test.py`：43 项通过。
- `py -3.9 tools\run_soc_host_c_test.py`：14 项通过。
- `py -3.9 tools\project_check.py`：78 项通过，0 warning，0 error。

## 后续建议

如果还要进一步降低复杂度，建议下一步只做结构分层，不改算法策略：把表驱动校准、静置 OCV、积分容量、发布显示分别拆成独立内部文件，并继续使用现有回放测试确认等效。
