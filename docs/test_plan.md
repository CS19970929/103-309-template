# 测试计划

文档状态：部分验证
最后更新时间：2026-06-02
说明：完整 review 后测试计划见 `docs/review/test_plan.md`；本文件按仓库协作规则保留为顶层入口。

## 状态变量净删减专项测试入口

专项审计文档：`docs/review/state_variable_audit.md`

当前阶段未修改源码，因此只执行文档和静态一致性检查。进入源码净删减后，必须至少覆盖：

| 测试项 | 入口 | 通过标准 |
|---|---|---|
| 文档一致性 | `rg "Q-SV-|REQ-SV-|SV-CLEAN" docs/review` | 审计、确认、风险、计划、测试文档都有入口 |
| 产品信息初始化收口 | `docs/review/test_plan.md#13-状态变量净删减专项测试` | `0xC002` 48 个寄存器读取不变 |
| FactoryAging 结构体收口 | `docs/review/test_plan.md#13-状态变量净删减专项测试` | 旧 `s_u*FactoryAging*` 符号无残留；老化状态、剩余时间、BKP/Flash 保存和 `0x14F80208` 行为不变 |
| LogRecord 结构体收口 | `docs/review/test_plan.md#13-状态变量净删减专项测试` | 旧日志私有状态符号无残留；startup/sleep/fault 日志和事件读取不变 |
| LedBar 初始化收口 | `docs/review/test_plan.md#13-状态变量净删减专项测试` | 上电显示、按键显示、TIM4 扫描、STOP 前 GPIO 行为不变 |
| `readyToSleep` 收口 | `docs/review/test_plan.md#13-状态变量净删减专项测试` | HICCUP/NORMAL/DEEP、sleep SOC、`BMS_SLEEP` 日志行为不变 |
| 基础静态检查 | `git diff --check`、仓库脚本、可用编译 | 结果与基线对比清楚，不把旧失败当成本次失败 |

硬件实测项仍以 `docs/review/test_plan.md` 为准。
