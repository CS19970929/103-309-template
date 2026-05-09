# SOC 真实骑行模拟测试报告

## 测试模型

- 电流输入使用 `A * 10`，与固件 `u16Ichg/u16IDischg` 一致。
- MCU SOC 安时积分节拍为 `200ms/5Hz`；快变电流测试按每个 200ms 样本平均电流校验。
- 电压由真实容量 SOC 反推 OCV，并叠加放电压降、充电极化和单体不一致。
- 被测对象是 `tools/soc_replay_test.py` 中镜像 `SocEnhance.c` 的主机模型。
- CSV 明细文件：`SOC_RIDE_SIM_SAMPLES.csv`。

## 汇总

| 工况 | 时长(s) | 真实SOC 起止 | 算法SOC 起止 | 显示SOC | 最大误差 | 最低Vmin | 最大电流(A*10) | 结果 | 说明 |
|---|---:|---:|---:|---:|---:|---:|---:|---|---|
| city_commute | 1320 | 80.0->61.54 | 80->62 | 62 | 0.50 | 3595 | 350 | PASS | 城市混合骑行应平滑跟随安时积分 |
| hill_climb | 720 | 60.0->46.30 | 60->46 | 46 | 0.50 | 3448 | 420 | PASS | 大电流压降不能误判空电 |
| fast_current_pulses | 360 | 70.0->63.94 | 70->64 | 64 | 0.50 | 3551 | 420 | PASS | 200ms/5Hz current pulse tracking should follow sampled average current |
| deep_cutoff | 900 | 18.0->1.83 | 18->0 | 0 | 2.49 | 2965 | 220 | PASS | 接近控制器截止电压时应收敛到零 |
| charge_anchor | 620 | 88.0->100.00 | 88->100 | 99 | 1.00 | 4050 | 270 | PASS | 满电电压锚点确认前不应发布未确认 100 |

## 复用方式

```powershell
py tools\soc_ride_sim_report.py --report SOC_RIDE_SIM_REPORT.md --csv SOC_RIDE_SIM_SAMPLES.csv
```

新增项目复用时，优先调整 `SCENARIOS` 中的容量、起始 SOC、工况段电流和压降参数，再用该报告对比固件变更前后的 SOC 轨迹。
