# SOC 测试脚本用途与调用边界

> 文档状态：已按当前脚本和源码核对
> 源码验证日期：2026-06-03
> 适用范围：`tools/soc_replay_test.py`、`tools/run_soc_host_c_test.py`、`tools/soc_visual_report.py`、`tools/project_check.py`

## 1. 结论

这些脚本有实际意义，但意义不同，不能混用结论：

- `soc_replay_test.py` 是 Python 模型回放，不是真实 C 固件验证。
- `run_soc_host_c_test.py` 会编译并运行真实 `SOC.c` 和 `SocEnhance.c`，但外围硬件/Flash/ADC/System 是 host stub。
- `soc_visual_report.py` 是真实 C host trace 的可视化趋势检查，适合看体验曲线，不适合作为硬门禁唯一依据。
- `project_check.py` 是仓库静态门禁，不是 SOC 功能测试；当前 checkout 有历史基线失败，必须按基线解释。

后续不应机械地“每次都全跑”。应按本轮改动类型选择最小有效验证集。

## 2. 脚本逻辑和实际意义

| 脚本 | 实际逻辑 | 有意义的地方 | 局限 |
|---|---|---|---|
| `tools/soc_replay_test.py` | 用 Python 重写一套 SOC 行为模型；读取 `Project_Config.h` 的部分宏；解析 `SocEnhance.c` 中活动 OCV/tail 表并和 Python 表断言一致 | 快速检查积分、OCV、tail、显示平滑、自耗口径等规则是否明显漂移；适合表值和参数改动后的快速回放 | 不运行真实 C；Python 模型本身可能和 C 实现不同步；不能证明 Keil/STM32/硬件路径正确 |
| `tools/run_soc_host_c_test.py` | 用 clang 编译真实 `SOC.c`、`SocEnhance.c` 和 `tools/soc_host_c_test.c`；host 侧提供 Flash/ADC/System/AFE sample seq stub；按 `0/30/1000mA` 自耗和 debug-watch 组合运行 | 覆盖真实 C 算法主体，比 Python replay 更可信；适合 SOC C 逻辑、自耗、RTC、Type-C、显示发布改动后验证 | 不是 Keil 编译；不跑真实 STM32 外设；Flash/ADC/RTC/CAN/系统错误都是 stub；不能替代上板 |
| `tools/soc_visual_report.py` | 编译真实 SOC C host trace，跑 city/hill/pulse/deep/charge 场景，输出 CSV/HTML 曲线报告 | 适合观察 SOC 内部值、显示值和真实场景曲线趋势；便于发现体验层突变 | 更偏可视化/趋势，不是完整断言集；通过不代表所有边界都正确 |
| `tools/project_check.py --quiet` | 静态扫描文件、宏、旧符号、文档和部分门禁规则 | 提交前发现明显旧符号回流、文档缺失、宏门禁问题 | 不是 SOC 功能测试；当前 checkout 有历史失败，不能把全仓失败直接归因到本轮 SOC 修改 |

## 3. 为什么之前会经常调用

之前经常调用这些脚本，是因为 SOC 模块改动集中在算法、tail、自耗、RTC 和发布口径上。为了避免“看起来只是简化写法，实际改变行为”，我把它们作为低成本回归手段使用。

但这不是硬性规则，也不是用户要求“每次都调用”。以后应避免把这些脚本变成机械动作，尤其不能把 Python replay 的通过结果包装成真实固件验证。

## 4. 后续调用规则

| 改动类型 | 建议验证 | 不需要默认执行 |
|---|---|---|
| 只改 SOC 文档、索引、说明文字 | `git diff --check`，必要时 `rg` 检查链接/旧术语 | 不默认跑 SOC replay、host C、visual trace |
| 改 SOC C 主流程、积分、自耗、RTC、Type-C、display publish | `python3 tools/run_soc_host_c_test.py`，必要时补跑 `soc_replay_test.py` | 不默认跑 visual trace，除非需要看趋势 |
| 改 OCV 表、empty-tail 表、时间参数 | `python3 tools/soc_replay_test.py`；若触及 C 逻辑，再跑 host C | mid-tail 已删除；不用 `project_check.py` 证明功能正确 |
| 调整用户体验曲线、快降/回弹/充电锚点体验 | `soc_replay_test.py` + `soc_visual_report.py`；必要时 host C | 不把 visual trace 当唯一硬门禁 |
| 提交前或跨模块较大变更 | `git diff --check`；可跑 `python3 tools/project_check.py --quiet` 并说明当前基线 | 不把历史失败算成本轮回归 |
| 出货/真板判断 | Keil `FD_Release`、真板充放电、RTC STOP 功耗、CAN/Modbus、Keil watch | 不用任何 host 脚本替代上板验证 |

## 5. 结论表达规则

以后汇报测试结果时必须区分：

- “Python 模型回放通过”：只说明模型和表/规则的一致性。
- “真实 C host 测试通过”：说明真实 `SOC.c/SocEnhance.c` 在 host stub 环境通过。
- “静态门禁通过/失败”：说明这是仓库规则检查，不是功能验证。
- “未做 Keil/真板验证”：必须明确写出，不能省略。

如果 `project_check.py` 仍是历史基线失败，应写成类似：

```text
python3 tools/project_check.py --quiet：当前 checkout 为历史基线失败，未见本轮 SOC 相关新增失败。
```

不要写成：

```text
SOC 已完整验证通过。
```

除非已经完成 Keil 编译和真板闭环验证。
