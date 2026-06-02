# 变更记录

文档状态：已按源码部分验证
最后更新时间：2026-06-02
说明：长期详细变更记录见 `docs/changelog/change_log.md`；本文件按仓库协作规则保留为顶层入口。

## 2026-06-02 状态变量净删减专项审计

本次只做文档和源码只读审查，未修改 `.c/.h`、Keil 工程、编译宏、协议、IAP/App 地址和烧录脚本。

更新内容：

- 新增 `docs/review/state_variable_audit.md`，按当前源码梳理 `s_ledbar.initialized`、`g_stLowPowerRtcStatus.readyToSleep`、LedBar 防抖状态、低功耗计时、日志边沿、系统状态、DataDeal 客户逻辑状态等变量。
- 更新 `docs/review/requirement_confirmation.md`，新增 `REQ-SV-*` 状态变量净删减需求确认表。
- 更新 `docs/review/requirement_questions.md`，新增 `Q-SV-*` 确认问题，并修正旧的 AFE/SOC 电流主路径描述：当前 `App_AFEGet()` 已调用 `DataLoad_Current()`。
- 更新 `docs/review/refactor_plan.md`，新增阶段 11 状态变量净删减专项阶段。
- 更新 `docs/review/risk_list.md`，新增状态变量净删减风险。
- 更新 `docs/review/test_plan.md`，新增状态变量净删减专项测试项。
- 更新 `docs/review/full_project_review.md`、`docs/review/module_map.md`、`docs/design/soc_design.md`、`docs/design/adc_afe_design.md`、`docs/review/document_source_consistency.md`，同步当前调度链与 AFE/SOC 数据流事实。

后续源码修改前必须先确认 `REQ-SV-*` 或 `Q-SV-*`。

## 2026-06-02 SV-01 产品信息初始化低风险净删减

本批次只处理 `ProductionID.c` 的一次性初始化 flag，不修改 Modbus/CAN 协议、`0xC002` 字段、产品信息默认值、Flash/IAP 地址、SOC、低功耗和 LED。

源码修改：

- `AppInit.c`：启动运行态初始化阶段显式调用 `InitProID()`。
- `ProductionID.h`：声明 `InitProID()`。
- `ProductionID.c`：删除 `App_ProID_Deal()` 内部 `su8_StartUpFlag` 和懒初始化逻辑；保留空 hook 供 `Runtime.c` 维持 `DBG_MODULE_PROID` heartbeat。

文档修改：

- 更新 `docs/review/state_variable_audit.md`、`docs/review/requirement_confirmation.md`、`docs/review/requirement_questions.md`、`docs/review/refactor_plan.md`、`docs/review/risk_list.md`、`docs/review/test_plan.md`、`docs/review/module_map.md`、`docs/reference/module_reference.md`，将 `Q-SV-004 / SV-01` 标记为已执行，并同步 PROID heartbeat hook 口径。

验证：

- `rg -n "su8_StartUpFlag" "103 + 309/Project/Source"`：源码无残留。
- `git diff --check`：通过。
- `clang -fsyntax-only` 检查 `AppInit.c` 和 `ProductionID.c`：通过。
- `python3 tools/project_check.py --quiet`：仍为仓库历史基线失败，本次结果 `88 OK / 1 warning / 40 errors`，失败项主要是历史缺文件、编码、配置宏/BuildGuard 检查和缺 `test_Autocurrent_cycle` 等，不是本批次新增问题。
- 未执行 Keil `FD_Release` 编译、真板运行或上位机 `0xC002` 读取。
