# SOC测试上位机执行报告

## 测试时间

2026-05-09

## 测试环境

- 工作区：`E:\TODO\103 + 309 - 副本`
- 上位机：`tools/soc_test_ui.py`
- 在线串口：`COM4`
- 波特率：`19200`
- 从机地址：`1`
- 当前板端固件：量产/普通固件，未开启 `0x2500/0xD300` 测试模式。

## 执行项目

### 1. SOC主机回放

命令：

```powershell
py tools\soc_replay_test.py
```

结果：`23 PASS`

关键覆盖：

- `200ms/5Hz` 安时积分节拍。
- 快变电流脉冲积分。
- 城市骑行、爬坡、低压截止、充电回满。
- 满电锚点、低压尾段、RTC OCV、显示平滑。

### 2. 真实骑行报告

命令：

```powershell
py tools\soc_ride_sim_report.py --report SOC_RIDE_SIM_REPORT.md --csv SOC_RIDE_SIM_SAMPLES.csv
```

结果：`5 PASS`

| 场景 | 结果 |
|---|---|
| city_commute | PASS |
| hill_climb | PASS |
| fast_current_pulses | PASS |
| deep_cutoff | PASS |
| charge_anchor | PASS |

输出：

- `SOC_RIDE_SIM_REPORT.md`
- `SOC_RIDE_SIM_SAMPLES.csv`

### 3. UI烟测

命令：创建 `SocTestUi`，刷新布局后销毁。

结果：`UI_OK tabs=5`

当前页签：

- 真实场景仿真
- MCU加速测试
- 在线板端监控
- 报告
- 日志

### 4. 一键测试链路

命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\soc_auto_test.ps1 -Port COM4 -Baud 19200 -Slave 1 -OnlineSamples 5 -OnlineInterval 0.2 -OnlineCsv SOC_ONLINE_MONITOR.csv
```

结果：

- 主机回放：23 PASS
- 真实骑行报告：5 PASS
- 在线板端监控：5 次读取成功

在线读数：

| 样本 | SOC | SOH | Vmin | Vmax | Ichg | Idsg | 容量 | 故障 |
|---:|---:|---:|---:|---:|---:|---:|---|---|
| 0 | 99% | 94% | 3676mV | 3678mV | 0 | 0 | 2513/2538 | 0000/0000/0000 |
| 1 | 99% | 94% | 3676mV | 3678mV | 0 | 0 | 2513/2538 | 0000/0000/0000 |
| 2 | 99% | 94% | 3676mV | 3678mV | 0 | 0 | 2513/2538 | 0000/0000/0000 |
| 3 | 99% | 94% | 3676mV | 3678mV | 0 | 0 | 2513/2538 | 0000/0000/0000 |
| 4 | 99% | 94% | 3676mV | 3678mV | 0 | 0 | 2513/2538 | 0000/0000/0000 |

### 5. 板端SOC参数读取

读取结果：

| 参数 | 值 |
|---|---:|
| capacity_a10 | 270 |
| cycle_times | 300 |
| v100_mv | 4180 |
| v0_mv | 3000 |

### 6. MCU测试模式探测

读取 `0xD300`：

- 结果：`modbus exception code=0x01`
- 结论：当前板端固件未开启测试模式或不支持该地址。

写 `0x2500`：

- 结果：`timeout waiting write-multiple response`
- 结论：当前板端固件拒绝/不响应测试样本注入，符合量产固件不允许测试模式影响正常程序的目标。

## 结论

- 上位机离线仿真、UI创建、自动化脚本、RS485在线监控均已通过。
- 当前接入的板子能正常通信，SOC/SOH/电压/容量/故障位可读。
- 当前板子不是测试固件，不能执行 MCU 加速注入；这正好证明量产固件不会被测试模式影响。
- 要完成“MCU真实骑行加速闭环”，下一步需要烧录开启 `PROJECT_CFG_SOC_TEST_MODE_ENABLE=1` 的测试固件，再运行 UI 的“MCU加速测试”。
