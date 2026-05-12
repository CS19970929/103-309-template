# SOC MCU真实骑行测试模式说明

## 目的

为了证明 SOC 在真实骑行电压、电流快速变化下仍然可信，新增 MCU 端测试模式和上位机加速测试入口。该模式只用于测试固件，不进入正常量产程序。

## 量产隔离

- 默认 `PROJECT_CFG_SOC_TEST_MODE_ENABLE=0`，量产固件不接受测试样本注入。
- `PROJECT_CFG_BUILD_PROFILE=Release` 时，如果测试模式被打开，`Project_BuildGuard.h` 会直接编译报错。
- 测试写入口为 `0x2500`，默认关闭时返回 `RS485_ERROR_NO_PERMISSION`，不会改写真实 AFE 电压、电流和 SOC。
- 正常在线监控仍读取 `0xD000`，不需要测试模式。

## 当前SOC积分频率

- MCU 每 `200ms` 运行一次 AFE 采样和 `App_SOC()`，安时积分频率为 `5Hz`。
- 固件积分常量：`SocEnhance.c` 中 `SOC_TICK_MS=200`，`SOC_TICKS_PER_SECOND=5`。
- 真实骑行电流变化快时，准确性取决于每个 `200ms` 样本是否代表这段时间的平均电流。若尖峰短于 200ms 且没有被电流采样/滤波平均进去，SOC 会出现采样混叠风险。

## MCU测试寄存器

### 写入加速样本

功能码：`0x10`，起始地址：`0x2500`，长度：6 个寄存器。

| 偏移 | 名称 | 单位 | 说明 |
|---:|---|---|---|
| 0 | enable | 0/1 | 1=启用并运行样本，0=关闭测试模式 |
| 1 | vmax_mv | mV | 单体最高电压 |
| 2 | vmin_mv | mV | 单体最低电压 |
| 3 | ichg_a10 | A*10 | 充电电流 |
| 4 | idsg_a10 | A*10 | 放电电流 |
| 5 | ticks | 200ms tick | 本样本在 MCU 内加速运行的 tick 数 |

默认单次最大 `ticks=300`，即 60 秒骑行时间。可通过 `PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX` 调整，但 Release 仍禁止开启测试模式。

### 读取测试状态

功能码：`0x03`，起始地址：`0xD300`，长度：16 个寄存器。

| 偏移 | 名称 | 说明 |
|---:|---|---|
| 0 | supported | 1=固件编译开启测试模式，0=量产/普通固件 |
| 1 | enabled | 当前是否处于测试模式 |
| 2 | tick_ms | 固定 200 |
| 3 | ticks_per_second | 固定 5 |
| 4 | max_ticks_per_write | 单次写入最大加速 tick |
| 5..9 | last sample | 最近一次 vmax/vmin/ichg/idsg/ticks |
| 10..11 | total_ticks | 已加速运行 tick 总数，高字在前 |
| 12 | soc | MCU 当前 SOC |
| 13 | soh | MCU 当前 SOH |
| 14 | cap_now_ah100 | 当前容量，Ah*100 |
| 15 | last_result | 0=OK，1=不支持，2=未初始化，3=参数非法 |

## 上位机使用流程

1. 烧录测试固件：Debug 或 Factory/Test profile，并显式开启 `PROJECT_CFG_SOC_TEST_MODE_ENABLE=1`。
2. 打开上位机：`.\tools\start_soc_test_ui.ps1`。
3. 在“在线板端监控”里确认串口、波特率、地址可读。
4. 进入“MCU加速测试”，先点“读取测试状态”，确认 `supported=1`、`tick=200ms`。
5. 点“运行MCU真实骑行”执行城市骑行、爬坡、高动态电流脉冲场景。
6. 点“运行快变电流”可单独验证 200ms/5Hz 下的快速电流变化积分误差。
7. 测试结束点“关闭测试模式”，再回到在线监控观察真实板端数据。

## 复用到后续项目

- MCU 端只需保留 `0x2500` 样本注入、`0xD300` 状态读取、Release 构建保护这三部分。
- 上位机复用 `tools/soc_online_monitor.py` 的 `run_mcu_soc_test_sample()` 和 `read_soc_test_status()`。
- 场景复用 `tools/soc_ride_sim_report.py` 的 `SCENARIOS`，根据新车型容量、最大电流、控制器限流和电压平台调整工况段。
