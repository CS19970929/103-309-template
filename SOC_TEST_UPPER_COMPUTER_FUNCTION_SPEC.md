# SOC测试上位机功能说明

## 目标定位

SOC测试上位机用于验证 BMS SOC 模块在真实骑行、充电、低压截止、快变电流和在线板端通信下是否可信。它不是只看一个 SOC 数字，而是把算法仿真、板端实时读数、测试固件加速注入和报告输出串成闭环。

## 当前功能

### 1. 真实场景仿真

入口：`tools/soc_test_ui.py` 的“真实场景仿真”页，或命令：

```powershell
py tools\soc_ride_sim_report.py --report SOC_RIDE_SIM_REPORT.md --csv SOC_RIDE_SIM_SAMPLES.csv
```

当前场景：

- `city_commute`：城市通勤，包含怠速、巡航、加速、短爬坡、停车。
- `hill_climb`：长爬坡，大电流压降后恢复。
- `fast_current_pulses`：快变电流脉冲，验证 `200ms/5Hz` 安时积分是否按采样平均电流计算。
- `deep_cutoff`：低 SOC 接近控制器截止电压，验证 SOC 收敛到 0。
- `charge_anchor`：充电回满，验证积分到 99 后必须等待满电电压锚点再到 100。

输出：

- `SOC_RIDE_SIM_REPORT.md`：汇总报告。
- `SOC_RIDE_SIM_SAMPLES.csv`：曲线原始数据。

### 2. 主机算法回放测试

入口：

```powershell
py tools\soc_replay_test.py
```

覆盖内容：

- 启动快照、OCV 启动、单次设置 SOC。
- 充放电安时积分、SOH/循环次数映射。
- 满电锚点、低压尾段、RTC 静置 OCV。
- 显示 SOC 平滑、固定/清零显示覆盖。
- 大电流压降 holdoff。
- 真实骑行、爬坡、低压截止、充电回满。
- `200ms/5Hz` 积分节拍和快变电流脉冲。

### 3. 在线板端监控

入口：UI 的“在线板端监控”页，或命令：

```powershell
py tools\soc_online_monitor.py --port COM4 --baud 19200 --slave 1 --samples 60 --interval 1 --csv SOC_ONLINE_MONITOR.csv
```

当前支持：

- 读取 `0xD000` 实时状态：单体最高/最低电压、充放电电流、SOC、SOH、容量、循环次数、故障位、均衡位。
- 读取 `0x2318~0x231B` SOC 参数：容量、循环次数、V100、V0。
- 写 `0x1005` 设置一次 SOC，UI 中有确认弹窗。
- 在线曲线展示并保存 CSV。

### 4. MCU加速测试

入口：UI 的“MCU加速测试”页。

用途：

- 连接测试固件后，通过 `0x2500` 注入真实骑行样本，让 MCU 端真实 SOC 算法加速运行。
- 通过 `0xD300` 读取测试模式状态、积分 tick、最大加速 tick、当前 SOC/SOH。
- 支持“运行MCU真实骑行”和“运行快变电流”。

限制：

- 当前量产固件默认不支持该模式，读 `0xD300` 或写 `0x2500` 被拒绝是预期结果。
- 要执行 MCU 注入测试，必须烧录 Debug 或 Factory/Test 测试固件，并开启 `PROJECT_CFG_SOC_TEST_MODE_ENABLE=1`。
- Release 量产构建中开启测试模式会直接编译报错，避免误出货。

### 5. 一键测试

入口：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\soc_auto_test.ps1 -Port COM4 -Baud 19200 -Slave 1 -OnlineSamples 60 -OnlineInterval 1
```

执行顺序：

1. `soc_replay_test.py` 主机回放。
2. `soc_ride_sim_report.py` 真实骑行报告。
3. 可选在线板端监控并输出 `SOC_ONLINE_MONITOR.csv`。

## 典型使用流程

### 量产固件确认

1. 打开 UI。
2. 在“在线板端监控”中扫描并选择串口。
3. 读取 SOC 参数。
4. 开始在线监控，确认 `SOC/SOH/Vmin/Vmax/Ichg/Idsg/故障位` 正常。
5. 切到“MCU加速测试”，读取测试状态。如果当前是量产固件，`0xD300` 不可读或 `supported=0` 属于正常。

### 测试固件验证真实骑行

1. 编译并烧录测试固件：`PROJECT_CFG_BUILD_PROFILE=Debug` 或 `Factory/Test`，`PROJECT_CFG_SOC_TEST_MODE_ENABLE=1`。
2. 打开 UI，确认串口在线。
3. “MCU加速测试”页读取状态，确认 `supported=1`、`tick=200ms`。
4. 点击“运行MCU真实骑行”，观察 SOC、电压、电流曲线。
5. 点击“运行快变电流”，重点确认大电流脉冲下 SOC 变化是否按平均电流积分。
6. 测试后点击“关闭测试模式”，再回到在线监控确认板端真实状态。

## 后续增强建议

- 把 `SOC_ONLINE_MONITOR.csv`、`SOC_RIDE_SIM_SAMPLES.csv` 和测试结论合并成一份 UI 内一键导出的完整报告。
- 增加测试固件自动识别：读 `0xD300` 成功后自动启用 MCU 加速按钮，否则置灰。
- 增加 CAN 口在线监控，用于对比 RS485 内部读数和对外广播是否一致。
- 增加断电恢复测试流程：记录断电前 SOC/容量，重启后读回并自动判定误差。
