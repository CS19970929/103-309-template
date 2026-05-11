# SOC 无板主机验证方案

本文说明在没有实物保护板时，如何在当前电脑上验证 SOC 模块逻辑可靠性，以及哪些内容仍必须上板确认。

## 1. 结论

可以在电脑上完整验证 SOC 算法的软件状态机，包括启动恢复、安时积分、OCV 表、满电确认、中低压弱约束、低压表、稳定静置、RTC 静置、回弹保护、显示平滑、异常电压和随机运行不变量。

当前最高可信度的无板验证入口是 `tools/run_soc_host_c_test.py`：它直接编译并执行 MCU 工程中的真实 `SOC.c`、`SocEnhance.c` 和 `PubFunc.c`，测试 harness 只替代 Flash、系统错误回调和全局采样结构。也就是说，SOC 主状态机、积分、OCV、低压、满电、显示发布等逻辑不是 Python 复刻，而是同一份 C 源码在电脑上运行。

不能只靠电脑证明硬件层完全可靠。以下内容仍需要板端或台架：

- AFE 实际电流零点、噪声、温漂和采样延迟。
- 电芯真实 OCV 曲线、温度、老化、内阻和压降回弹曲线。
- 内部 Flash 在真实供电跌落、复位、擦写寿命下的行为。
- RS485/CAN/LED 的实际电气、时序和外设中断交互。
- Keil 完整链接、下载、量产烧录与 IAP 地址安全。

所以当前策略是：Host C 真实源码测试作为第一门禁，Python 回放矩阵作为广覆盖补充；板端测试作为采样、通信、存储和用户体验最终验收。

## 2. 当前主机验证入口

```bash
python3 tools/run_soc_host_c_test.py
python3 tools/soc_replay_test.py
python3 tools/project_check.py
git diff --check
clang -fsyntax-only -std=c99 -Wall -Wextra \
  -DSTM32F10X_HD -DUSE_STDPERIPH_DRIVER \
  -I"103 + 309/Project/Source" \
  -I"103 + 309/Project/Source/conf" \
  -I"103 + 309/Project/Source/easylogger/inc" \
  -I"103 + 309/Project/Users" \
  -I"103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers" \
  -I"103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc" \
  -I"103 + 309/Libraries/CMSIS/CM3/CoreSupport" \
  -I"103 + 309/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x" \
  -I"103 + 309/Libraries/STM32F10x_StdPeriph_Driver/inc" \
  "103 + 309/Project/Source/SOC.c" \
  "103 + 309/Project/Source/SocEnhance.c"
```

`tools/run_soc_host_c_test.py` 当前覆盖 `14` 个真实 C 源码场景：启动 OCV、放电积分、Type-C 电流抵消、满电确认、低压到 0、稳定静置 deferred target、充/放电阶段消化 OCV 差值、RTC 稳定窗口、久置低 OCV 慢速下修、静置超过 30min 但电压不稳定时不校准、重载回弹标志清除、显示覆盖不污染内部 SOC、设置一次 SOC 保存快照。

`tools/soc_replay_test.py` 当前覆盖 `43` 个场景。该脚本用 Python 镜像 `SocEnhance.c` 的核心决策，适合快速跑完所有 SOC 软件等价类。

## 3. 覆盖矩阵

| 类别 | 已覆盖内容 |
| --- | --- |
| 真实 C 源码路径 | host 直接编译 `SOC.c` + `SocEnhance.c` + `PubFunc.c`，验证 `InitData_SOC()` / `App_SOC()` / `SOC_IntEnhance_Ctrl()` 的真实调用链 |
| 启动恢复 | 无快照默认 60%、无快照有效电压按 OCV、V2 快照恢复 SOC/容量/循环/SOH、重载回弹标志恢复 |
| 安时积分 | 200ms/5Hz 积分、充电/放电方向、脉冲电流平均能量、容量边界、循环小数累计 |
| SOH | 循环到 SOH 映射、80% 下限、SOH 变化后有效容量约束 |
| OCV 表 | 表项精确命中、全电压范围单调性、与 C 源码表格一致性、静置/RTC deferred target、方向约束 |
| 满电 | 快速/普通确认、V100 参数影响、计数器递减而非清零、满电前不直接显示 100 |
| 中低压弱约束 | `3500mV~3700mV` 全表目标/周期、重载禁用、压差门控、条件中断计数清零 |
| 低压表 | `3400mV~2950mV` 全表目标/周期、四个电流档位、只下修不上拉、末端快速到 0 |
| 大电流回弹 | `Idsg > C/2` sag holdoff、真实末端放行、跨重启 5min rebound holdoff、归零即清标志 |
| 稳定静置 | 稳定 5min 后按 10min/step 记录 OCV target，短静置不立即改 SOC；方向匹配的充/放电阶段再消化差值 |
| 长时间不用车 | 稳定电压长时间 RELAX 且 OCV target 低于内部 SOC 时，按 30min/1% 慢速下修；如果电压超过 30min 仍不稳定，也不会到点强校准 |
| 显示 | 内部 SOC 与显示 SOC 分离，普通 5s/1%，低压 1s/1%，Fixed/Zero 不破坏内部 SOC |
| 异常输入 | 0mV、反向电压、超上限、压差过大等坏样本不触发电压校准 |
| 随机矩阵 | 20000 tick 的充/放/静置/坏电压组合，持续检查 SOC、显示、容量、SOH、计数器不变量 |

## 4. 新增 SOC 逻辑时必须补的测试

1. 先在 `tools/soc_host_c_test.c` 增加真实 C 源码场景，证明 MCU SOC 主路径行为正确。
2. 新增自动校准路径时，必须加入“单次最多 `1%`”测试。
3. 新增电压表或阈值时，必须加入全表矩阵测试，而不是只测一个典型点。
4. 新增静置或休眠逻辑时，必须同时测“满足条件”和“不满足条件不校准”。
5. 新增快照字段时，必须测启动恢复、未知值掩码、保存触发和断电后重启语义。
6. 新增显示策略时，必须确认内部 SOC、显示 SOC、通信输出不会互相污染。

## 5. 用户自测方式

没有板子时，推荐每次改 SOC 后按以下顺序执行：

```bash
python3 tools/run_soc_host_c_test.py
python3 tools/soc_replay_test.py
python3 tools/project_check.py
git diff --check
```

有 Windows + Keil 环境时，再补 `FD_Debug` 和 `FD_Release` 完整编译。有实物板时，使用 `tools/start_soc_test_ui.ps1` 启动 SOC 测试上位机做串口在线监控或 demo；Host C 测试本身是命令行门禁，没有 UI，因为它的目的不是展示，而是让 CI/提交前快速失败。

## 6. 无板验证边界

Host C 测试和主机回放只能证明“给定输入下，SOC 状态机输出符合预期”。它不证明输入一定真实。因此上线前仍建议用台架补以下数据：

- 不同 SOC、不同电流下的单体电压下陷和回弹时间。
- 停车 5min、10min、30min、6h 后的实际 OCV 回归程度。
- 控制器欠压保护点附近，SOC 是否能在保护前收敛到用户可接受范围。
- 满电后停充、充电器浮充、充电器断开时 SOC 到 100 的体验。
- CAN/RS485/LED 显示是否与 `g_stCellInfoReport.SocElement` 一致。
