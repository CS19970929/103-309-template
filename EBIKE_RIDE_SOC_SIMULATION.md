# Ebike 骑行 SOC 模拟说明

## 目的

用于在没有真实电池包和电子负载的情况下，把一组可重复的 ebike 骑行电压、电流输入注入到现有 SOC 算法前端，验证库仑积分、OCV 修正、满电确认和弱单体保护对 SOC 的影响。

默认固件不启用该功能。只有打开 `PROJECT_CFG_EBIKE_RIDE_SIM_ENABLE` 后，`App_AFEGet()` 才会在真实 AFE/ADC 采样完成后覆盖 `g_stCellInfoReport` 中的单体电压、总压、充放电电流，再递增 `g_u32AfeCurrentSampleSeq`，因此 `App_SOC()` 会把模拟值当作正常采样值处理。

## 开关

配置文件：`103 + 309/Project/Source/conf/Project_Config.h`

- `PROJECT_CFG_EBIKE_RIDE_SIM_ENABLE`
  - `0`：关闭，正常使用真实 AFE/ADC 采样。
  - `1`：开启模拟。Release 构建会被 `Project_BuildGuard.h` 拦截，测试时应切到 Debug 或 Factory/Test profile。
- `PROJECT_CFG_EBIKE_RIDE_SIM_PROFILE`
  - `0`：自动骑行工况，包含静置、起步、巡航、爬坡、轻载回弹和小电流充电。
  - `1`：恒定 8A 放电，便于核对库仑积分速度。
  - `2`：手动调试器输入，通过 `EbikeRideSim_SetManualSample(cell_mv, ichg_a10, idsg_a10)` 注入单点。
- `PROJECT_CFG_EBIKE_RIDE_SIM_INITIAL_SOC_PERCENT`
  - 模拟器内部真实 SOC 初值。
- `PROJECT_CFG_EBIKE_RIDE_SIM_CELL_RES_MOHM`
  - 单体等效内阻，决定负载压降和充电抬升。
- `PROJECT_CFG_EBIKE_RIDE_SIM_CELL_IMBALANCE_MV`
  - 单体间最大模拟不一致电压。

## 单位

- 单体电压：`mV`
- 总压 `u16VCellTotle`：`10mV`
- 充电/放电电流：`A * 10`
  - `80` 表示 `8.0A`
  - `300` 表示 `30.0A`
- 模拟器真实容量：`A * 10 * s`，与 `SocEnhance.c` 内部容量单位一致。

## 观察点

调试器里查看全局变量：

- `g_stEbikeRideSimObserve.u8TruthSoc`：模拟器内部真实 SOC。
- `g_stEbikeRideSimObserve.u16CellOcv_mV`：按真实 SOC 生成的静态单体 OCV。
- `g_stEbikeRideSimObserve.u16CellLoad_mV`：叠加内阻压降/抬升后的负载单体电压。
- `g_stEbikeRideSimObserve.u16Ichg_A10` / `u16IDsg_A10`：当前模拟电流。
- `g_stCellInfoReport.SocElement.u16Soc`：现有 SOC 算法输出。

测试时重点比较：

```text
SOC 误差 = g_stCellInfoReport.SocElement.u16Soc - g_stEbikeRideSimObserve.u8TruthSoc
```

## 注意事项

1. 开启模拟后，真实 AFE 电压和电流会被覆盖，只能用于台架和固件算法测试。
2. 不要同时注入充电和放电电流，手动接口检测到两者同时非零时会保留充电、清零放电。
3. 测试前建议清除或重置 SOC 快照，否则 Flash 中旧 SOC 会影响初始误差。
4. 自动工况的 OCV 表没有直接复用 SOC 估算表，避免测试结果过度乐观。
5. 若要模拟新的路况，只需要调整 `EbikeRideSim.c` 中 `s_stAutoProfile` 的分段时长、电流和额外压降。
