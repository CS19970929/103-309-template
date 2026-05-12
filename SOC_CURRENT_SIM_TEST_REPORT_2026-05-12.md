# SOC 当前代码模拟测试报告（2026-05-12）

## 测试范围

本次按当前工作树直接测试 SOC 模块，重点覆盖：

- 真实 C 源码路径：`SOC.c` + `SocEnhance.c`，通过宿主机编译运行。
- Python 场景矩阵：启动、积分、满电、低压、中低压弱约束、静置/RTC OCV、回弹保护、显示平滑、异常电压和随机运行不变量。
- 骑行曲线模拟：城市混合骑行、爬坡大电流、快速脉冲、深度放电、充电满电锚点。

当前工作树中 `PROJECT_CFG_SOC_REST_OCV_SECONDS=60`，测试按这个当前配置执行。

## 发现的问题

首次执行 `py tools\run_soc_host_c_test.py` 时，真实 C 源码宿主测试失败 5 项，集中在稳定静置/RTC OCV 校准：

- 稳定静置后 deferred OCV target 未能在后续充电阶段上修 1%。
- 稳定静置后 deferred OCV target 未能在后续放电阶段下修 1%。
- RTC 稳定窗口未能在后续充电阶段消化 OCV 差值。
- 久置低 OCV 未能按预期慢速下修。

根因是 `SocEnhance.c` 中 `stable_rest_ticks` 计数上限直接使用 `PROJECT_CFG_SOC_REST_OCV_SECONDS`。当前该值为 `60s`，低于内部静置可信门槛 `SOC_SHORT_REST_MIN_SECONDS=300s`，导致稳定静置计数永远达不到触发条件，运行静置和 RTC OCV 校准路径实际失效。

## 修正内容

1. `SocEnhance.c`
   - 新增 `soc_rest_stable_limit_seconds()`。
   - 将稳定静置窗口计数上限调整为 `max(PROJECT_CFG_SOC_REST_OCV_SECONDS, SOC_SHORT_REST_MIN_SECONDS)`。
   - `rest_ticks` 仍保留原配置上限，用于久置低 OCV 下修门槛；稳定可信窗口不再被过小配置截断。

2. `tools\soc_replay_test.py`
   - 改为读取 `Project_Config.h` 中的 SOC 校准参数。
   - Python 回放不再固定使用旧的 `REST_OCV_SECONDS=1800`。
   - 同步修正稳定静置窗口上限和久置下修测试期望。

3. `tools\soc_host_c_test.c`
   - 久置低 OCV 测试按当前配置计算起始等待时间，兼容 `REST_OCV_SECONDS` 小于 `600s` 的情况。

## 实测结果

已执行命令：

```powershell
py tools\run_soc_host_c_test.py
py tools\soc_replay_test.py
py tools\soc_ride_sim_report.py --report build\SOC_RIDE_SIM_REPORT_current.md --csv build\SOC_RIDE_SIM_SAMPLES_current.csv
py tools\project_check.py
git diff --check
```

结果：

| 项目 | 结果 |
| --- | --- |
| 真实 C 源码宿主测试 | 14/14 通过 |
| Python SOC 回放矩阵 | 43/43 通过 |
| 骑行曲线模拟 | 5/5 通过 |
| 工程自动检查 | 78 OK，0 warning，0 error |
| diff 空白检查 | 通过，仅 Git 提示 LF/CRLF 转换 warning |

骑行曲线摘要：

| 工况 | 算法 SOC | 最大误差 | 结论 |
| --- | ---: | ---: | --- |
| city_commute | 80 -> 62 | 0.50% | 通过 |
| hill_climb | 60 -> 46 | 0.50% | 通过 |
| fast_current_pulses | 70 -> 64 | 0.50% | 通过 |
| deep_cutoff | 18 -> 0 | 2.49% | 通过，接近欠压时可收敛到 0 |
| charge_anchor | 88 -> 100 | 1.00% | 通过，满电电压锚点生效 |

## 当前结论

从无板模拟看，当前 SOC 软件状态机在给定输入下逻辑成立：

- 安时积分节拍和容量换算正确，200ms/5Hz 脉冲电流能按采样平均值积分。
- 满电确认有效，未满足电压锚点前不会直接发布未确认的 100%。
- 低压表和中低压弱约束能把高估 SOC 向安全方向收敛。
- 大电流 sag/rebound holdoff 能阻止爬坡压降误判为空电，同时末端低压仍允许收敛。
- 稳定静置/RTC OCV 校准已恢复可用，并且仍保持每次最多 1% 的小步修正。
- 显示 SOC 与内部 SOC 分离，`SOC_Fixed/SOC_Zero` 不污染内部容量状态。

准确性边界仍然存在：这些测试证明“当前代码在模拟输入下行为正确”，不能替代真实电芯 OCV 曲线、温度、老化、AFE 零点漂移、Flash 擦写和 RS485/CAN/LED 实物验证。上板前仍建议用 `tools\start_soc_test_ui.ps1` 做 COM4/19200/slave=1 在线监控，并读取 `0xD000`、`0xD300` 确认量产测试入口隔离状态。
