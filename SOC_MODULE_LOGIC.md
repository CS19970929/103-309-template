# SOC 模块逻辑说明

本文按 2026-05-12 当前工作区源码重新梳理 SOC 模块。结论以代码为准，主要依据：

- `103 + 309/Project/Source/SOC.c`
- `103 + 309/Project/Source/SOC.h`
- `103 + 309/Project/Source/SocEnhance.c`
- `103 + 309/Project/Source/SocEnhance.h`
- `103 + 309/Project/Source/Flash.c`
- `103 + 309/Project/Source/Flash.h`
- `103 + 309/Project/Source/DataDeal.c`
- `103 + 309/Project/Source/Sci_Upper.c`
- `103 + 309/Project/Source/Sci_Upper.h`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/SleepDeal.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/conf/Project_Config.h`
- `103 + 309/Project/Source/conf/Project_BuildGuard.h`
- `tools/run_soc_host_c_test.py`
- `tools/soc_replay_test.py`
- `tools/soc_online_monitor.py`

## 1. 当前结论

当前 SOC 是以安时积分为主、端点/低压/静置 OCV 小步校正为辅的工程化状态机。内部 SOC 与对外显示 SOC 分离，内部容量和循环用于计算，`g_stCellInfoReport.SocElement` 发布的是经过显示平滑或显示覆盖后的结果。

核心约束：

1. `App_SOC()` 只在 AFE 电流新样本到达时推进完整算法，避免重复积分旧样本。
2. 采样周期按 `200ms` 计算，`SOC_TICKS_PER_SECOND = 5`。
3. 自动校正每次最多移动 `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT`，当前为 `1%`。
4. 充电积分不能直接到 `100%`，必须通过满电电压确认后逐步锚定。
5. 低压和中低压修正只会向下限制明显高估 SOC，不会把 SOC 向上拉高。
6. 静置/RTC OCV 只接受低于当前 SOC 的 deferred target，不允许向上校准；后续放电阶段或久置低 OCV 才小步下修。
7. 大电流压降会进入 sag/rebound holdoff，阻止把未回弹端电压当作真实 OCV。
8. SOC 快照使用内部 Flash V2 journal，并兼容旧 V1 快照。
9. 量产默认关闭 SOC 注入测试入口，`0xD300 supported=0` 是正常结果。

## 2. 安全边界

量产和测试必须隔离：

- 量产配置必须保持 `PROJECT_CFG_BUILD_PROFILE 0`。
- 量产配置必须保持 `PROJECT_CFG_SOC_TEST_MODE_ENABLE 0`。
- SOC 注入测试固件才允许使用 `PROJECT_CFG_BUILD_PROFILE 2`、`PROJECT_CFG_SOC_TEST_MODE_ENABLE 1`、`PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX 300`。
- `Project_BuildGuard.h` 会在 SOC 测试入口开启但构建 profile 不是 Factory/Test 时直接编译报错。

烧录安全：

- IAP/Bootloader 地址是 `0x08000000`。
- 正常 App 地址是 `0x08004800`。
- App scatter 文件是 `103 + 309/Project/Users/Objects/FD_Release.sct`。
- 禁止把 `FD_Debug.bin` 或 `FD_Release.bin` 裸写到 `0x08000000`。
- App 烧录优先使用：

```powershell
.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash
```

上位机启动：

- 不直接双击 `tools\soc_test_ui.py`。
- 固定使用 `.\tools\start_soc_test_ui.ps1`。
- 自动演示使用：

```powershell
.\tools\start_soc_test_ui.ps1 -Demo -Port COM4 -Baud 19200 -Slave 1 -Samples 10 -Interval 0.5
```

## 3. 模块边界

| 文件 | 当前责任 |
| --- | --- |
| `SOC.c` | SOC 门面层；加载 `OtherElement` 和 SOC 表；处理 Type-C 等效电流；按 AFE 样本序号调用算法；实现 SOC 测试注入口 |
| `SOC.h` | SOC 表大小、默认表声明、`InitData_SOC()` / `App_SOC()` / 测试 API 声明 |
| `SocEnhance.c` | SOC 主状态机；负责积分、SOH、满电、低压、中低压、静置/RTC OCV、显示平滑、快照 |
| `SocEnhance.h` | 对外结构 `SOC_ENHANCE_ELEMENT`、表选择枚举、状态机 API |
| `DataDeal.c` | AFE 采样、最大/最小单体、电流计算；每 200ms 增加 `g_u32AfeCurrentSampleSeq` 后调用 `App_SOC()` |
| `Flash.c/.h` | SOC V2 快照 journal；V1 兼容读取；内部 Flash 地址定义 |
| `Sci_Upper.c/.h` | RS485/Modbus-like 读写窗口；SOC 表、SOC 参数、一次 SOC、SOC 测试模式 |
| `rtc_sleep.c` | RTC 周期唤醒后累计休眠秒数，调用 `SOC_ApplyRtcRelaxationCompensation()` |
| `SleepDeal.c` | 正常休眠前保存 SOC 快照和灯条 SOC 备份 |
| `LedBar.c` | 读取 `SocElement.u16Soc` 做数码管/灯条显示；休眠前把显示 SOC 存到 BKP |
| `Can_HDX.c` | CAN 对外上报 SOC、SOH、剩余容量、满电容量、循环次数 |

## 4. 当前默认配置

| 项目 | 当前值 | 来源与说明 |
| --- | ---: | --- |
| 构建 profile | `0` | `PROJECT_CFG_BUILD_PROFILE`，量产 Release |
| SOC 测试入口 | `0` | `PROJECT_CFG_SOC_TEST_MODE_ENABLE`，量产关闭 |
| SOC 单次注入 tick 上限 | `300` | 每 tick 为 `200ms`，测试固件才生效 |
| 电芯体系 | `TERNARYLI` | `PROJECT_CFG_BAT_CHEMISTRY=0` |
| 电池板类型 | `BAT_SLAVE` | `PROJECT_CFG_BAT_TYPE=1` |
| 串数 | `10` | `SNum=10`，`SeriesNum` 默认 10 |
| 标称容量 | `27Ah` | `BMS_CAPCITY=270`，单位 `10 * Ah` |
| 默认循环次数 | `3` | `OtherElement.u16Soc_Cycle_times` |
| SOC 表选择 | `SOC_TABLE_TERNARYLI` | `OtherElement.u16Soc_TableSelect` |
| 满电电压 | `4180mV` | `OtherElement.u16Soc_V_100` |
| 空电显示端点 | `3000mV` | `OtherElement.u16Soc_V_0` |
| 满电普通确认 | `15s` | `PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS` |
| 满电快速确认 | `5s` | `PROJECT_CFG_SOC_FULL_CONFIRM_FAST_SECONDS` |
| 满电普通最低 SOC | `95%` | 防止中低电量误锚定 100% |
| 满电普通电压裕量 | `80mV` | `V100 - 80mV = 4100mV` |
| 满电快速电压裕量 | `30mV` | `V100 - 30mV = 4150mV` |
| 满电最大压差 | `120mV` | 单体压差门控 |
| 校准有效电压 | `2000~5000mV` | 单体最小/最大有效范围 |
| 校准最大压差 | `1000mV` | 通用电压校准门控 |
| 中低压/静置最大压差 | `200mV` | 更严格的弱约束门控 |
| 静置稳定波动 | `30mV` | `VCellMin/VCellMax` 相对参考值 |
| 静置 OCV 配置时间 | `30min` | `PROJECT_CFG_SOC_REST_OCV_SECONDS`，达到后才允许久置低 OCV 慢速下修 |
| 静置 OCV 最小稳定 | `5min` | `SOC_SHORT_REST_MIN_SECONDS` |
| OCV 目标刷新节拍 | `10min` | `SOC_SHORT_REST_STEP_SECONDS` |
| 久置低 OCV 下修节拍 | `30min/1%` | `SOC_LONG_REST_DOWN_STEP_SECONDS` |
| 大电流 holdoff | `30s` | `PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS` |
| holdoff 允许末端偏移 | `50mV` | `V0 + 50mV` 以下允许低压收敛 |
| 重载重启回弹保护 | `5min` | 快照 `u16Flags bit0` 恢复后启用 |
| Type-C 输出电压 | `9000mV` | `TYPEC_OUT_VOLTAGE_MV` |
| Type-C DC/DC 效率 | `90%` | `TYPEC_DCDC_EFFICIENCY_PERMILLE=900` |
| 显示平滑 | `5s/1%` | 普通上升/下降 |
| 低压显示下降 | `1s/1%` 或 `0.2s/1%` | 低压末端加速显示跟随 |

## 5. 启动与运行链路

启动顺序：

1. `main()` 调用 `InitDevice()`。
2. `InitDevice()` 先执行 `InitE2PROM()`，加载默认参数和内部 Flash RW 参数。
3. 初始化 AFE、CAN、ADC、SCI。
4. 调用 `InitData_SOC()`，此处必须在参数读取之后。
5. `InitData_SOC()` 从 `OtherElement` 复制容量、循环、表选择、V100、V0 和 `SOC_Table_Set`。
6. `soc_param_lib_init()` 初始化 `SOC_STATE`，读取或创建 SOC 快照，并强制发布一次 SOC。

运行链路：

1. 主循环 `Runtime_RunOnce()` 调用 `SysTime_LatchTaskFlags()`。
2. `App_AFEGet()` 通过 `SysTime_Take200msTaskPeriod()` 保证 200ms 周期。
3. AFE 刷新单体电压、总压、温度和电流。
4. `g_u32AfeCurrentSampleSeq++`，序号溢出到 0 时会再加一次，避免 0。
5. `App_AFEGet()` 末尾调用 `App_SOC()`。
6. `App_SOC()` 只有发现样本序号变化才调用 `SOC_UpdateSampleData()` 和 `SOC_IntEnhance_Ctrl()`；否则只发布已有 SOC。

`Runtime_RunOnce()` 中单独的 `App_SOC()` 当前被注释，SOC 推进由 AFE 200ms 样本驱动。

## 6. 输入数据

电压输入来自 `g_stCellInfoReport`：

- `u16VCellMax`：最大单体电压，mV。
- `u16VCellMin`：最小单体电压，mV。
- `u16VCellDelta`：单体压差，mV。
- `u16VCellTotle`：总压，单位 `V * 100`，换算包电压时乘以 `10` 得 mV。

电流输入：

- `u16Ichg`：充电电流，单位 `A * 10`。
- `u16IDischg`：放电电流，单位 `A * 10`。
- 有效方向阈值为 `2`，即 `0.2A`。

Type-C 输出侧电流会折算为电池侧等效放电电流：

```text
pack_mv = g_stCellInfoReport.u16VCellTotle * 10
I_bat_mA = I_typec_out_mA * TYPEC_OUT_VOLTAGE_MV * 1000
           / (pack_mv * TYPEC_DCDC_EFFICIENCY_PERMILLE)
I_bat_A10 = round(I_bat_mA / 100)
```

`SOC_GetNetCurrentForCalc()` 用 `AFE 充电电流 - AFE 放电电流 - Type-C 等效放电电流` 得到最终积分电流：

- 净值为正：作为 `soc_ichg`。
- 净值为负：绝对值作为 `soc_idsg`。
- 这样 Type-C 对外供电会抵消充电，或叠加到放电。

## 7. 内部状态与单位

`SocEnhance.c` 只保留一个 `SOC_STATE`：

| 字段 | 含义 |
| --- | --- |
| `cap_factory_as10` | 出厂容量，单位 `As * 10`；`27Ah` 对应 `270 * 3600` |
| `cap_full_as10` | 当前有效满容量，按 SOH 从出厂容量折算 |
| `cap_now_as10` | 当前剩余容量 |
| `cycle_x100` | 循环次数，内部单位 `cycle * 100` |
| `dsg_acc_as10` | 未满 `1%` 循环的放电累计 |
| `rem_ms` | 200ms 积分余量 |
| `soc` | 内部 SOC |
| `display_soc` | 对外显示 SOC |
| `soh` | 当前 SOH |
| `mode/last_mode` | `RELAX / CHG / DSG` |
| `full_ticks` | 满电确认计数 |
| `empty_ticks` | 低压表下修计数 |
| `mid_ticks` | 中低压弱约束下修计数 |
| `rest_ticks/stable_rest_ticks/short_rest_ticks` | 静置可信和 OCV 目标刷新计数 |
| `long_rest_down_ticks` | 久置低 OCV 慢速下修计数 |
| `sag_hold_ticks` | 大电流压降/重启回弹保护计数 |
| `deferred_ocv_target/deferred_ocv_valid/deferred_ocv_ticks` | 静置/RTC 生成的待下修 OCV 目标；目标高于或等于当前 SOC 时直接忽略 |
| `rest_ref_vmin/rest_ref_vmax` | 静置稳定性参考电压 |
| `snapshot_flags` | 需要持久化的状态标志，当前 bit0 为 rebound hold |
| `full_anchor` | 是否已经通过满电锚定到 100% |

对外 `SocElement` 字段：

- `u16Soc`：显示 SOC，百分比。
- `u16Soh`：SOH，百分比。
- `u16CapacityNow`：剩余容量，单位 `Ah * 100`，等价于 `10mAh`。
- `u16CapacityFull`：当前满电容量，单位 `Ah * 100`。
- `u16CapacityFactory`：出厂容量，单位 `Ah * 100`。
- `u16Cycle_times`：对外整数循环次数。

## 8. 快照与掉电恢复

SOC 快照结构是 `STORAGE_FLASH_SOC_DATA` V2，存储在内部 Flash journal：

| Slot | 地址 |
| --- | --- |
| A | `0x0801E000` |
| B | `0x0801E800` |

当前使用字段：

| 字段 | 用途 |
| --- | --- |
| `u16FormatVersion` | V2 固定写 `0x0002` |
| `u16SocNow` | 内部 SOC |
| `u16DsgSocInt` | 放电循环小数百分比兼容字段 |
| `u16MaxErrorPercent` | 固定写 `100`，兼容字段 |
| `u32CycleTimes` | `cycle * 100` |
| `u32CapNow` | 当前剩余容量 |
| `u32CapFull` | 当前有效满容量 |
| `u32LearnPassedAs10` | 未满 `1%` 循环的放电累计 |
| `u16Flags bit0` | 重载/大电流后回弹保护标志 |

恢复规则：

1. 优先按 V2 长度和 magic 读取 journal。
2. 如果 V2 不存在，再尝试读取旧 V1 结构，并迁移到 V2 内存格式。
3. 快照有效条件包含 `u16SocNow <= 100`、`u16DsgSocInt <= 100`。
4. 若 `u32CapNow` 有效且不超过 `cap_full_as10`，按容量恢复内部 SOC。
5. 否则按 `u16SocNow` 恢复，并同步 `cap_now_as10`。
6. 若 `u16Flags bit0` 存在，启动后设置 `5min` rebound holdoff。
7. 无有效快照时，如果当前电压有效，则按 OCV 表估算启动 SOC；否则使用默认 `60%`，并立即保存快照。

保存触发：

- 内部 SOC 改变。
- 循环次数改变。
- SOH 变化导致 `cap_full_as10` 改变。
- `snapshot_flags` 改变。
- 设置一次 SOC、容量/循环重置、RTC 静置修正等需要强制落盘的动作。
- 进入休眠前调用 `SOC_SaveSnapshotBeforeSleep()`。

## 9. OCV 表

默认三元锂表：

```text
4160/100, 4100/95, 4050/90, 3995/85, 3935/80,
3880/75, 3835/70, 3795/65, 3760/60, 3725/55,
3695/50, 3670/45, 3645/40, 3615/35, 3585/30,
3555/25, 3525/20, 3480/15, 3400/10, 3250/5,
3000/0
```

表选择：

| 枚举 | 值 | 来源 |
| --- | ---: | --- |
| `SOC_TABLE_TEST` | `0` | 上位机写入的 `SOC_Table_Set` |
| `SOC_TABLE_LIFEPO` | `1` | 固件内置磷酸铁锂表 |
| `SOC_TABLE_TERNARYLI` | `2` | 当前默认三元锂表 |
| `SOC_TABLE_LIFEPO2` | `3` | 固件内置备用磷酸铁锂表 |

`0x2200` 起写 42 个寄存器只更新 RAM 中的 `SOC_Table_Set` 并重新初始化 SOC；当前没有把 SOC 表写入 Flash。`SOC_Table_Set` 只有在 `OtherElement.u16Soc_TableSelect=SOC_TABLE_TEST` 时才作为 OCV 表使用。表选择本身位于 `OtherElement` 参数区偏移 12，对应 `0x230C`，会随 RW 参数保存到内部 Flash。

OCV 表使用线性插值。它只在这些场景参与：

- 无有效快照时的启动估算。
- 稳定静置/RTC 生成 deferred target。
- 手动刷新标志触发的受限 OCV 小步修正。

骑行瞬态不会用 OCV 表直接覆盖 SOC。

## 10. 安时积分

方向判断：

| 条件 | 模式 |
| --- | --- |
| `Ichg >= 0.2A` 且 `Ichg >= Idsg` | `CHG` |
| `Idsg >= 0.2A` | `DSG` |
| 其他 | `RELAX` |

方向切换时清空 `rem_ms`，避免余量跨方向继承。

积分公式：

```text
acc_ms = current_A10 * 200 + rem_ms
delta_as10 = acc_ms / 1000
rem_ms = acc_ms % 1000
```

处理规则：

- `CHG` 增加 `cap_now_as10`，上限为 `cap_full_as10`。
- `DSG` 减少 `cap_now_as10`，同时累计 `dsg_acc_as10` 和循环。
- `RELAX` 不积分，并清空 `rem_ms`。
- 如果充电积分算到 `100%`，但 `full_anchor` 还没建立，会回退到 `99%`，等待满电电压确认。

## 11. SOH 与循环

SOH 当前只按循环次数映射，不做 FCC 学习：

```text
drop = floor(cycle / 100)
SOH = max(100 - drop, 80)
cap_full = cap_factory * SOH / 100
```

循环计数：

```text
unit = cap_factory_as10 / 100
每累计 unit 放电量，cycle_x100 += 1
对外 cycle_times = cycle_x100 / 100
```

默认循环次数为 `3`，因此初始 SOH 仍为 `100%`。当 SOH 因循环数下降时，会重新计算 `cap_full_as10`，并用当前容量重新得到内部 SOC。

## 12. 校准统一门控

通用电压校准门控：

- `VCellMin >= 2000mV`。
- `VCellMax <= 5000mV`。
- `VCellMax >= VCellMin`。
- `VCellMax - VCellMin <= 1000mV`。
- 若 `PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT=1`，三级保护故障会阻止校准。
- 若 `PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT=1`，AFE/ADC/CBC/温度断线等系统故障会阻止校准。

中低压弱约束和静置 OCV 还要求：

- 单体压差 `<=200mV`。
- 不处于 sag/rebound holdoff。

sag/rebound holdoff：

- `Idsg > C/2` 时触发，当前 `C/2 = 13.5A`，即 `Idsg > 135`。
- 触发后保持 `30s`，并把 `u16Flags bit0` 写入快照。
- 如果重载后关机，下次开机恢复 `u16Flags bit0` 后进入 `5min` rebound holdoff。
- holdoff 期间只在 `VCellMin > V0 + 50mV` 时阻止电压类校准。
- 如果 `VCellMin <= V0 + 50mV`，认为已经进入真实末端，低压表仍可收敛到 0。

## 13. 满电确认

满电确认在非 `DSG` 模式下运行，即 `CHG` 和充电停止后的 `RELAX` 都可确认。

基础条件：

- 通用电压校准门控通过。
- `VCellMax >= V100 - 80mV`，当前为 `4100mV`。
- 单体压差最终要求 `<=120mV`。

快速确认：

- `VCellMin >= V100 - 30mV`，当前为 `4150mV`。
- 单体压差 `<=120mV`。
- 条件持续 `5s` 后，把内部 SOC 朝 `100%` 小步推进。

普通确认：

- 内部 SOC `>=95%`。
- `VCellMin >= V100 - 80mV`，当前为 `4100mV`。
- 单体压差 `<=120mV`。
- 条件持续 `15s` 后，把内部 SOC 朝 `100%` 小步推进。

计数方式：

- 满足条件时 `full_ticks++`。
- 不满足但计数不为 0 时 `full_ticks--`，不是一次抖动直接清零。
- 每次锚定最多移动 `1%`。
- 到达 `100%` 后设置 `full_anchor=1`。

## 14. 低压表

低压表在非充电状态运行，包括 `DSG` 和放电停止后的 `RELAX`。它根据 `V0 + offset` 和电流档位给 SOC 设置保守上限，只向下修正，不向上拉高。

电流档位：

| 档位 | 条件 |
| --- | --- |
| `RELAX` | 当前模式为 `RELAX` |
| 轻载 | `Idsg <= C/5`，当前约 `5.4A` |
| 中载 | `Idsg <= C/2`，当前约 `13.5A` |
| 重载 | `Idsg > C/2` |

默认 `V0=3000mV` 时低压表如下。周期表示每下修 `1%` 所需时间：

| VCellMin | RELAX 目标/周期 | 轻载目标/周期 | 中载目标/周期 | 重载目标/周期 |
| --- | --- | --- | --- | --- |
| `<=2950mV` | `0% / 0.2s` | `0% / 0.2s` | `0% / 0.2s` | `0% / 0.2s` |
| `<=2975mV` | `0% / 1s` | `0% / 1s` | `0% / 0.2s` | `0% / 0.2s` |
| `<=3000mV` | `0% / 2s` | `0% / 1s` | `0% / 1s` | `0% / 1s` |
| `<=3050mV` | `4% / 4s` | `5% / 3s` | `8% / 2s` | `12% / 1.6s` |
| `<=3100mV` | `8% / 7s` | `10% / 6s` | `14% / 5s` | `18% / 4s` |
| `<=3200mV` | `12% / 12s` | `14% / 10s` | `20% / 8s` | `25% / 6s` |
| `<=3300mV` | `14% / 18s` | `18% / 15s` | `25% / 12s` | `32% / 9s` |
| `<=3400mV` | `18% / 24s` | `22% / 20s` | `30% / 16s` | `40% / 12s` |

低压表活跃时：

- 不叠加静置 OCV。
- 每个调度最多一个校准动作。
- 若处于 holdoff 且 `VCellMin > V0 + 50mV`，低压表不参与。
- 若已进入 `V0 + 50mV` 以下，允许继续向 0 收敛。

## 15. 中低压弱约束

中低压弱约束用于限制明显高估 SOC，范围是 `V0 + 400mV < VCellMin <= V0 + 700mV`。默认 `V0=3000mV` 时，实际覆盖 `3400mV~3700mV` 之间，不包含低压表已经覆盖的 `<=3400mV`。

启用条件：

- 非 `CHG`。
- 电压有效。
- 单体压差 `<=200mV`。
- 不处于 sag/rebound holdoff。
- `VCellMin > V0 + 400mV`。

默认表如下。周期表示每下修 `1%` 所需时间：

| VCellMin | RELAX 目标/周期 | 轻载目标/周期 | 中载目标/周期 | 重载 |
| --- | --- | --- | --- | --- |
| `<=3500mV` | `25% / 90s` | `32% / 90s` | `42% / 120s` | 禁用 |
| `<=3600mV` | `35% / 120s` | `42% / 120s` | `50% / 150s` | 禁用 |
| `<=3650mV` | `45% / 150s` | `50% / 150s` | `58% / 180s` | 禁用 |
| `<=3700mV` | `55% / 180s` | `60% / 180s` | 禁用 | 禁用 |

SOC 低于目标时不动作；条件中断时 `mid_ticks` 清零。

## 16. 静置与 RTC OCV

静置 OCV 不再按“到 30min 直接按表校准”。当前逻辑先判断电压是否稳定，再生成 deferred target，并且默认不立即修改内部 SOC。

运行态 `RELAX` 路径：

1. 只有 `RELAX` 时累计静置；进入 `CHG/DSG` 会清空静置可信计数。
2. 电压必须通过校准门控，且压差 `<=200mV`。
3. 第一次稳定采样会记录 `rest_ref_vmin/rest_ref_vmax`。
4. 后续 `VCellMin/VCellMax` 相对参考值波动 `<=30mV` 才继续累计稳定。
5. 稳定至少 `5min` 且 `10min` 刷新节拍到达时，记录一次 OCV deferred target。
6. 记录 target 不会立即修改内部 SOC。
7. target 高于内部 SOC 时，只在后续 `CHG` 阶段按 `10min/1%` 上修。
8. target 低于内部 SOC 时，只在后续 `DSG` 阶段按 `10min/1%` 下修。
9. 如果后续方向与 target 不匹配，target 会被丢弃。
10. 若 target 低于内部 SOC 且一直稳定久置，静置阶段也允许按 `30min/1%` 慢速下修。

当前默认配置下，第一次久置低 OCV 下修的有效时间约为：

```text
10min 生成 deferred target + 30min 慢速下修节拍 = 约 40min 下修 1%
```

RTC 休眠路径：

1. 进入 RTC/STOP 前调用 `SOC_SaveSnapshotBeforeSleep()`。
2. 每次 RTC 唤醒后累计 `s_u32RtcSleepElapsedSeconds`。
3. `update_rtc_soc()` 调用 `SOC_ApplyRtcRelaxationCompensation(rest_seconds, vmin, vmax)`。
4. RTC 路径复用同一套稳定电压、deferred target、久置慢速下修规则。
5. 第一次 RTC 唤醒通常只建立参考，不立即校准。
6. 若中途电压跳动超过 `30mV`，会重置稳定窗口和 target。

CAN active/idle 只影响 RTC 唤醒服务频率，不直接改变 SOC 校准速率。

## 17. 显示与对外发布

内部 SOC 和显示 SOC 分离：

- `s_soc.soc` 是内部计算值。
- `s_soc.display_soc` 是发布给 `g_stCellInfoReport.SocElement.u16Soc` 的显示值。
- 容量、SOH、循环次数仍来自内部状态。

显示目标：

| 条件 | 显示目标 |
| --- | --- |
| `System_OnOFF_Func.bits.b1OnOFF_SOC_Zero=1` | `0%` |
| `System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed=1` | `60%` |
| 其他 | 内部 SOC |

显示跟随：

| 场景 | 跟随速度 |
| --- | --- |
| 普通上升或下降 | `5s / 1%` |
| 低压下降且 `VCellMin <= V0 + 50mV` | `1s / 1%` |
| 极低压下降且 `VCellMin <= V0 - 50mV` | `0.2s / 1%` |
| 初始化、设置一次 SOC、显示覆盖 | 强制同步显示目标 |

`SOC_Fixed` 和 `SOC_Zero` 只影响显示，不修改内部 SOC 或容量快照。

`LedBar.c` 从 `g_stCellInfoReport.SocElement.u16Soc` 读取显示值，并在休眠前把当前显示 SOC 写入 `BKP_DR4/BKP_DR5`，用于唤醒前预览。

## 18. RS485/Modbus-like 通信

读状态：

| 地址 | 内容 |
| --- | --- |
| `0xD000` | 基础只读窗口，当前基区 97 个 word |
| `0xD000 + 52` | `SocElement.u16Soc` |
| `0xD000 + 53` | `SocElement.u16Soh` |
| `0xD000 + 54` | `SocElement.u16CapacityNow` |
| `0xD000 + 55` | `SocElement.u16CapacityFull` |
| `0xD000 + 56` | `SocElement.u16CapacityFactory` |
| `0xD000 + 57` | `SocElement.u16Cycle_times` |
| `0xD300` | SOC 注入测试状态窗口，16 个 word |

`0xD300` 状态 word：

| word | 含义 |
| ---: | --- |
| 0 | `PROJECT_CFG_SOC_TEST_MODE_ENABLE`，量产为 0 |
| 1 | 测试模式当前 enabled 状态 |
| 2 | tick 毫秒数，固定 200 |
| 3 | 每秒 tick 数，固定 5 |
| 4 | 单次写入最大 tick 数，当前 300 |
| 5 | 上次注入 `vcell_max` |
| 6 | 上次注入 `vcell_min` |
| 7 | 上次注入 `ichg` |
| 8 | 上次注入 `idsg` |
| 9 | 上次注入 `ticks` |
| 10..11 | 累计注入 tick，高低 16 位 |
| 12 | 当前 SOC |
| 13 | 当前 SOH |
| 14 | 当前剩余容量 |
| 15 | 结果码，量产为 unsupported |

写入口：

| 功能 | 地址 | 说明 |
| --- | --- | --- |
| 设置一次 SOC | `0x1005` | `0..100`，设置 `u16_RefreshData_Flag=3`，下个 SOC 调度落盘 |
| 写 SOC 表 | `0x2200` 起 42 word | 只更新 RAM 表并 `InitData_SOC()`，不持久化 |
| 写 SOC 表选择 | `0x230C` | `0..3`，属于 `OtherElement`，保存到内部 Flash RW 参数区 |
| 写 SOC 容量/循环/V100/V0 | `0x2318~0x231B` | 属于 `OtherElement`，保存到内部 Flash RW 参数区 |
| SOC 注入样本 | `0x2500` 起 6 word | `[enable, vmax, vmin, ichg, idsg, ticks]`，量产无权限 |
| 固定 SOC 显示 | 系统功能开关 | 只影响显示目标 60% |
| SOC 清零显示 | 系统功能开关 | 只影响显示目标 0% |

`0x2500` 校验：

- word 数必须为 6。
- `ticks` 必须 `1..PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX`。
- `1000mV <= vcell_min <= vcell_max <= 5000mV`。
- `ichg/idsg <= 5000`。
- SOC 必须已经初始化完成。

## 19. CAN 输出

CAN 输出读取的是 `g_stCellInfoReport.SocElement`：

- 飞道扩展帧 `feidao_send_soc_1000ms()`：发送充电状态、SOC、温度、电池类型等。
- 飞道扩展帧 `feidao_send_cap_5000ms()`：发送实时能量和设计能量。
- 飞道扩展帧 `feidao_send_soh_5000ms()`：发送 SOH 和循环次数。
- 标准帧 `CAN_TX_0x00()`：总压、电流、剩余容量。
- 标准帧 `CAN_TX_0x01()`：满电容量、循环次数、SOC。
- 标准帧 `CAN_TX_0x05()`：SOH。

因此 CAN 侧看到的 SOC 与 RS485 `0xD000+52` 一致，都是显示 SOC。

## 20. 上位机与验证脚本

在线监控脚本：

```powershell
py -3.9 tools\soc_online_monitor.py --port COM4 --baud 19200 --slave 1 --samples 30 --interval 1
```

脚本默认读取：

- `0xD000` 63 个寄存器，解析 SOC、SOH、容量、电压、电流和故障。
- `0x2318` 4 个寄存器，解析容量、循环、V100、V0。
- `0xD300` 16 个寄存器，解析 SOC 测试入口状态。

设置一次 SOC 对应协议是 `0x06` 写单寄存器 `0x1005`，值为 `0..100`。`tools\soc_online_monitor.py` 内部提供 `set_soc_once()` 辅助函数，但当前 CLI 入口主要用于在线读取；需要图形界面或自动演示时，使用 `tools\start_soc_test_ui.ps1`。

量产固件验收时，读取 `0xD300` 得到 `supported=0` 是正确结果，表示注入式 SOC 测试入口关闭。

## 21. 已验证项目

本次重梳理后已在当前工作区执行：

```powershell
py -3.9 tools\run_soc_host_c_test.py
py -3.9 tools\soc_replay_test.py
```

结果：

- `SOC host C tests passed: 14`
- `SOC replay tests passed: 43`

真实 C 源码主机测试直接编译 `SOC.c` 和 `SocEnhance.c`，覆盖：

- 启动 OCV。
- 放电积分。
- Type-C 输出侧电流折算为电池侧等效电流。
- 满电确认。
- 低压到 0。
- 稳定静置 deferred target。
- RTC 稳定窗口。
- 久置低 OCV 慢速下修。
- 电压不稳定时不校准。
- 重载回弹标志清除。
- 显示覆盖不污染内部 SOC。
- 设置一次 SOC 保存快照。

Python 回放矩阵覆盖 43 个场景，包括：

- 快照有效/无效恢复。
- OCV 表单调性和 C 源码表一致性。
- 200ms 积分和方向阈值。
- 满电确认计数器递减。
- 低压表和中低压表全矩阵。
- 大电流 sag holdoff。
- rebound holdoff 跨重启。
- 静置/RTC OCV deferred target。
- 随机运行不变量。

Keil 完整编译和上板 RS485/CAN 验证仍需实物与 Keil MDK 环境。

## 22. 上板复核清单

量产固件：

1. 确认 `PROJECT_CFG_BUILD_PROFILE 0`。
2. 确认 `PROJECT_CFG_SOC_TEST_MODE_ENABLE 0`。
3. 使用安全脚本烧录 App 到 `0x08004800`。
4. 通过 `COM4/19200/slave=1` 读取 `0xD000`。
5. 核对 `SOC/SOH/CapacityNow/CapacityFull/CapacityFactory/Cycle`。
6. 读取 `0xD300`，确认 `supported=0`。

SOC 测试固件：

1. 只在 Factory/Test profile 下开启 `PROJECT_CFG_SOC_TEST_MODE_ENABLE 1`。
2. 通过 `0x2500` 注入 `[enable, vmax, vmin, ichg, idsg, ticks]`。
3. 通过 `0xD300` 核对 last sample、total ticks、SOC、last result。
4. 测试结束恢复量产配置，并再次确认 `0xD300 supported=0`。

## 23. 当前风险与后续方向

已知设计取舍：

- SOH 只是循环次数映射，不代表真实 FCC 学习。
- SOC 表只适合当前电芯和温度/倍率假设，运行态已通过门控避免直接跳变，但上板仍需按实际电芯校准。
- `0x2200` 写入的 SOC 表不持久化，重启后会回到默认表或当前表选择对应的内置表。
- `SOC_Fixed` / `SOC_Zero` 是显示覆盖，不应作为真实 SOC 校准使用。
- 当前静置 OCV 第一次 target 约在稳定 10min 后生成，久置低 OCV 第一次自动下修约在 40min 后发生，适合保守显示，不适合快速台架调参。

后续如果要继续优化，应优先补充：

- 实车放电到保护前的 `VCellMin/Idsg/SOC` 曲线。
- 充满静置后的 `V100` 和回落曲线。
- 不同温度下的 OCV 表偏差。
- Type-C 大功率输出与 AFE 电流同时存在时的实测误差。
- CAN 与 RS485 同步读取的一致性记录。
