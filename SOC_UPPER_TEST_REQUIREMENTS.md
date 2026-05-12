# SOC 自动化测试与上位机需求整理

> 最新完整需求、功能和实测报告见：
> - `SOC_TEST_REQUIREMENTS_SUMMARY.md`
> - `SOC_TEST_UPPER_COMPUTER_FUNCTION_SPEC.md`
> - `SOC_TEST_EXECUTION_REPORT.md`
> - `SOC_MCU_RIDE_TEST_MODE.md`

## 目标

把 SOC 模块测试拆成两条线：

1. 主机加速仿真：不接板子，模拟真实骑行电压/电流，快速回归算法。
2. 在线板端测试：接 RS485/CAN，读取 MCU 实时状态，必要时下发测试命令并生成报告。

这两条线后续可以合并成专用上位机：仿真负责提前筛问题，在线测试负责验证固件和硬件闭环。

## MCU 端需要提供的能力

### 必须提供

1. 实时状态读取
   - `VCellMax/VCellMin/VCellDelta`
   - 总压
   - 充电电流 `Ichg`、放电电流 `Idsg`
   - SOC、SOH、当前容量、满容量、出厂容量、循环次数
   - 三级故障位、均衡位
   - 当前工程已有：RS485 `0x03` 读取 `0xD000` 共 63 个 halfword。

2. SOC 参数读取
   - 容量 `0.1Ah`
   - 循环次数
   - 满电端点 `V100`
   - 空电端点 `V0`
   - 当前工程已有：RS485 `0x03` 读取 `0x2318~0x231B`。

3. 单次 SOC 设置
   - 用于验证内部 SOC、显示 SOC、容量同步。
   - 当前工程已有：RS485 `0x06` 写 `0x1005`。

4. OCV 表读取/写入
   - 用于验证不同电芯体系或客户曲线。
   - 当前工程已有：`0x2200` 起 42 个 halfword。

### 建议新增

1. SOC 调试只读区
   - 内部 SOC、显示 SOC 分开输出。
   - `cap_now_as10/cap_full_as10/dsg_acc_as10/rem_ms`
   - `full_ticks/empty_ticks/rest_ticks/sag_hold_ticks`
   - 当前模式 `RELAX/CHG/DSG`
   - 最近一次校准原因：积分、满电、低压表、静置 OCV、RTC OCV、上位机设置。

2. 测试模式开关
   - 允许上位机注入虚拟电压/电流样本，不依赖真实电池负载。
   - 注入样本仍走 `SOC_UpdateSampleData()` 和 `SOC_IntEnhance_Ctrl()`，这样能测试 MCU 端真实逻辑。
   - 必须仅在工厂/调试固件打开，量产默认关闭。

3. 快速时间倍率
   - 例如 `1x/10x/60x`，只影响 SOC 测试定时器，不影响保护动作。
   - 用于把 30min 静置 OCV、长骑行、低压收敛测试压缩到几分钟。

4. 测试结果快照
   - 输出最近一次 SOC 保存成功/失败、Flash slot、序列号。
   - 用于断电恢复和 Flash 快照寿命测试。

## 上位机测试软件形态

### 当前脚本能否演进为上位机

可以。当前脚本已经具备上位机核心能力：

- `tools/soc_replay_test.py`：主机 SOC 算法回归测试。
- `tools/soc_ride_sim_report.py`：真实骑行电压/电流模拟并输出 Markdown/CSV 报告。
- `tools/soc_online_monitor.py`：通过 RS485 在线读取板端状态并保存 CSV。
- `tools/soc_auto_test.ps1`：一键执行回放、加速仿真和可选在线监控。
- `tools/soc_test_ui.py`：图形化 SOC 测试上位机，集成场景仿真、曲线、在线监控和报告查看。
- `tools/start_soc_test_ui.ps1`：固定图形化上位机启动入口，默认使用已验证的 Windows Python Launcher `py -3.9`，避免双击脚本时进入错误 Python 环境。

后续图形化上位机可以直接复用这些协议和场景定义，界面只负责选择串口、选择场景、启动测试、展示曲线和导出报告。

### 建议功能

1. 串口连接
   - 自动扫描 COM 口。
   - 默认 `19200 8N1`、从机地址 `1`。
   - 支持读取 `0xD000`、`0x2318`、`0x2200`。
   - GUI 当前已支持 `0xD000` 在线监控、`0x2318~0x231B` 参数读取、`0x1005` 设置一次 SOC。
   - 写 `0x1005` 会改变板端当前 SOC，上位机必须弹窗确认，并在写入后立即读回 `0xD000` 验证。

2. 场景库
   - 城市骑行：怠速、巡航、加速、短爬坡、停车。
   - 长爬坡：大电流、压降、松油恢复。
   - 低压截止：接近控制器保护电压时 SOC 应收敛到 0。
   - 充电回满：积分到 99 后必须等待满电电压锚点确认到 100。
   - 静置恢复：稳定窗口建立 deferred OCV target，后续充/放电方向匹配时消化；久置低 OCV 按 30min/1% 下修。

3. 加速测试
   - 主机仿真天然加速，不等待真实时间。
   - MCU 在线加速需要固件提供“虚拟样本注入”和“时间倍率”。

4. 报告输出
   - Markdown 汇总。
   - CSV 原始曲线。
   - PASS/FAIL 门槛、最大 SOC 误差、最低单体电压、最大电流、最终显示 SOC。

## 当前已完成的测试覆盖

1. 主机回放测试
   - 启动快照、OCV 启动、设置一次 SOC。
   - 充/放电积分、SOH 映射。
   - 满电确认、低压尾段、RTC 静置 OCV。
   - 显示平滑、工厂固定/清零显示覆盖。
   - 大电流 voltage sag holdoff。
   - 城市骑行、爬坡、低压截止、充电回满。

2. 在线读板测试
   - 已通过 `COM4 / 19200 / slave=1` 读取板端 `0xD000`。
   - 读到 SOC=100、SOH=97、容量 `2619/2619/2700 Ah*100`、循环 300。
   - 连续 10 次状态读取稳定，故障位为 0。

## 一键复用命令

主机自动化：

```powershell
.\tools\soc_auto_test.ps1
```

启动图形化上位机：

```powershell
.\tools\start_soc_test_ui.ps1
```

启动图形化上位机并自动演示在线监控：

```powershell
.\tools\start_soc_test_ui.ps1 -Demo -Port COM4 -Baud 19200 -Slave 1 -Samples 10 -Interval 0.5
```

主机自动化 + 在线读板：

```powershell
.\tools\soc_auto_test.ps1 -Port COM4 -Baud 19200 -OnlineSamples 60 -OnlineInterval 1
```

单独生成真实骑行模拟报告：

```powershell
py tools\soc_ride_sim_report.py --report SOC_RIDE_SIM_REPORT.md --csv SOC_RIDE_SIM_SAMPLES.csv
```

单独在线监控：

```powershell
py tools\soc_online_monitor.py --port COM4 --baud 19200 --samples 60 --interval 1 --csv SOC_ONLINE_MONITOR.csv
```
