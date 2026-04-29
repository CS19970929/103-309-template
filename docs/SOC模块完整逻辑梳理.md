# SOC 模块完整逻辑梳理

本文基于当前工作区 `103 + 309/Project/Source` 源码梳理，重点覆盖 `SOC.c`、`SocEnhance.c/.h`、`Flash.c/.h`、`EEPROM.c`、`Sci_Upper.c`、`rtc_sleep.c` 以及 SOC 对外消费者。当前日期：2026-04-29。

## 1. 总体结论

当前 SOC 模块不是单纯电压查表，也不是单纯安时积分，而是一个组合策略：

1. 主估算路径是 200ms 周期的安时积分。
2. 电压只在可信条件下参与修正，包括满电确认、静置 OCV、RTC 休眠补偿、轻载在线 OCV、弱单体保护。
3. 内部 SOC 与对外显示 SOC 分离，内部值可按算法修正，对外 `g_stCellInfoReport.SocElement.u16Soc` 会平滑发布。
4. SOC 运行快照保存到后 64K Flash 的双槽 journal 区，启动优先恢复快照；无有效快照时使用默认 60%。
5. 容量学习只在可信满电/空电锚点之间发生，并限制学习范围和单次变化幅度。
6. 上位机可以写 SOC 表、基础容量参数、一次性设 SOC，并通过 refresh flag 触发不同重算路径。

## 2. 文件边界

| 文件 | 职责 |
| --- | --- |
| `SOC.c` | SOC 调度外壳。加载 `OtherElement` 和 `SOC_Table_Set` 到 `SOC_Enhance_Element`，提供 `InitData_SOC()` 和 `App_SOC()`。 |
| `SOC.h` | SOC 表长度、边界常量、外部入口声明。 |
| `SocEnhance.h` | 对外结构体 `SOC_Enhance_Element`、SOC 表枚举、公共算法 API。 |
| `SocEnhance.c` | SOC 算法主体：状态机、安时积分、OCV 查表、端点修正、静置修正、在线 OCV、弱单体保护、容量学习、显示平滑、快照触发。 |
| `Flash.h/.c` | SOC 快照结构体 V2、后 64K Flash 地址、双槽 journal、CRC、sequence、V1 兼容迁移。 |
| `EEPROM.c/.h` | 历史命名仍为 EEPROM，当前负责 RW 参数默认值、Flash 参数加载保存、升级策略下 SOC 配置/快照重置。 |
| `DataDeal.c/.h` | AFE 电压/电流采样输入来源，定义 `OtherElement` 中 SOC 基础参数。 |
| `Sci_Upper.c/.h` | 上位机读写入口：SOC 表、SOC 基础参数、一次性设 SOC、系统开关副作用。 |
| `rtc_sleep.c` | RTC 休眠唤醒后调用 SOC 静置补偿。 |
| `Can_HDX.c`、`LedBar.c`、`Fault.c` | 使用 SOC 对外结果做 CAN 上报、灯条显示、低 SOC 故障判断。 |

## 3. 当前编译配置视角

当前 `Project_Config.h` 中和 SOC 直接相关的编译期配置如下：

| 配置项 | 当前值 | 影响 |
| --- | ---: | --- |
| `PROJECT_CFG_BAT_TYPE` | `1` | `BAT_SLAVE`，对应 `BMS_CAPCITY = 270`，单位为 `10 * Ah`，即 27.0Ah。 |
| `PROJECT_CFG_BAT_CHEMISTRY` | `0` | 启用 `TERNARYLI` 默认参数。 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV` | `80` | 满电确认时最低单体要至少接近 `V100 - 80mV`，且会受化学体系下限约束。 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV` | `120` | 满电确认允许的最大单体压差。 |
| `PROJECT_CFG_SOC_ONLINE_OCV_GUARD_ENABLE` | `1` | 启用轻载在线 OCV 小步融合。 |
| `PROJECT_CFG_SOC_ONLINE_OCV_CORRECTION_SECONDS` | `30` | 在线 OCV 条件持续 30s 后才修正 1%。 |
| `PROJECT_CFG_SOC_ONLINE_OCV_MIN_DELTA_PERCENT` | `3` | 在线 OCV 目标和内部 SOC 差值必须大于 3%。 |
| `PROJECT_CFG_SOC_ONLINE_OCV_CURRENT_DIVIDER` | `10` | 在线 OCV 轻载上限为 `C/10`，默认约 2.7A。 |
| `PROJECT_CFG_SOC_ONLINE_OCV_HEAVY_DSG_CURRENT_DIVIDER` | `3` | 重载放电阈值为 `C/3`，默认约 9.0A。 |
| `PROJECT_CFG_SOC_ONLINE_OCV_HEAVY_DSG_HOLDOFF_SECONDS` | `180` | 重载放电后 180s 内禁止在线 OCV 融合。 |
| `PROJECT_CFG_SOC_ONLINE_OCV_STABLE_SECONDS` | `20` | 在线 OCV 要求电压稳定 20s。 |
| `PROJECT_CFG_SOC_ONLINE_OCV_STABLE_WINDOW_MV` | `8` | 在线 OCV 稳定窗口为 8mV。 |
| `PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV` | `2000` | 电压类校准允许的最低单体电压。 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV` | `5000` | 电压类校准允许的最高单体电压。 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV` | `1000` | 电压类校准允许的最大单体压差。 |
| `PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT` | `0` | 当前三级保护故障不阻塞 SOC 电压类校准。 |
| `PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT` | `0` | 当前 AFE/ADC/CBC/温度系统故障不阻塞 SOC 电压类校准。 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_TABLE` | `0` | 升级策略不重置自定义 SOC 表。 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_CONFIG` | `1` | 升级策略版本变化时重置 SOC 基础配置。 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_SNAPSHOT` | `1` | 升级策略版本变化时重置 SOC 快照为默认 60%。 |

`Project_BuildGuard.h` 对上述配置做编译期范围检查，例如在线 OCV 修正周期必须在 1 到 600s，校准电压上下限必须合理，开关类只能为 0 或 1。

## 4. 默认参数视角

当前默认值由 `DataDeal.h` 的 `OtherElement_default` 决定。当前配置为 `BAT_SLAVE + TERNARYLI` 时：

| 字段 | 默认值 | 含义 |
| --- | ---: | --- |
| `u16Soc_TableSelect` | `SOC_TABLE_TERNARYLI` | 默认使用三元锂内置 OCV 表。 |
| `u16Soc_Ah` | `270` | 27.0Ah，单位 `10 * Ah`。 |
| `u16Soc_Cycle_times` | `3` | 出厂循环次数初值。 |
| `u16Soc_V_100` | `4180` | 满电单体电压端点，mV。 |
| `u16Soc_V_0` | `3000` | 空电单体电压端点，mV。 |
| `SNum` | `10` | 默认串数。 |

SOC 支持四类 OCV 表：

| 表选择 | 表来源 | 说明 |
| --- | --- | --- |
| `SOC_TABLE_TEST` | `SOC_Table_Set[42]` | 上位机可写的自定义表。 |
| `SOC_TABLE_LIFEPO` | `SOC_Table_LiFePO[42]` | 内置磷酸铁锂表。 |
| `SOC_TABLE_TERNARYLI` | `SocTable_TernaryLi[42]` | 内置三元锂表，当前默认。 |
| `SOC_TABLE_LIFEPO2` | `SocTable_LiFePO2[42]` | 另一套磷酸铁锂表。 |

查表使用 `GetEndValue()`。表按 `[电压, SOC, 电压, SOC, ...]` 成对存放，函数会在线段内线性插值，超出表范围时返回边界 SOC。

## 5. 数据结构视角

### 5.1 对外交互结构 `SOC_Enhance_Element`

定义在 `SocEnhance.h`，是 `SOC.c`、`SocEnhance.c` 和外部模块之间的桥梁。

| 字段 | 含义 |
| --- | --- |
| `u16_SOC_Ah` | 配置容量，单位 `10 * Ah`。 |
| `u16_SOC_CycleT_Ever` | 配置中的已有循环次数。 |
| `u16_SOC_CycleT_Limit` | 循环寿命限制，当前加载时固定 5000。 |
| `u16_SOC_TableSelect` | OCV 表选择。 |
| `u16_SOC_0_Vol` / `u16_SOC_100_Vol` | 0%/100% 端点单体电压，mV。 |
| `SOC_Table_CanSet[42]` | 自定义 SOC 表副本。 |
| `u8_SetSocOnce` | 上位机一次性设置 SOC 的目标值。 |
| `u16_VCellMax` / `u16_VCellMin` | 当前最大/最小单体电压。 |
| `u16_Ichg` / `u16_Idsg` | 当前充/放电电流，单位 `A * 10`。 |
| `u16_SOC_InitOver` | SOC 初始化完成标志。 |
| `u8_SOC` | 对外发布 SOC，已经过显示平滑。 |
| `u8_SOH` | 对外发布 SOH。 |
| `u16_CapacityNow` / `u16_CapacityFull` / `u16_CapacityFactory` | 对外发布容量，单位 `Ah * 100`，等价 10mAh。 |
| `u16_Cycle_times` | 对外发布循环次数。 |
| `u8_SOC_OCV_Cali` | 当前复用为放电 SOC 百分比累加器导出值。 |
| `u16_RefreshData_Flag` | `1` OCV 刷新，`2` 容量基准重置，`3` 一次性设 SOC。 |

### 5.2 内部计算结构 `SOC_Calculate_Element`

定义在 `SocEnhance.c`，只在算法内部使用。

| 字段 | 含义 |
| --- | --- |
| `u32CapFactory` | 出厂容量，内部单位 `As * 10`。由 `u16_SOC_Ah * 3600` 得到。 |
| `u32CapFull` | 当前满充容量基准，内部单位 `As * 10`。用于 SOH 和积分百分比。 |
| `u32CapNow` | 当前剩余容量，内部单位 `As * 10`。 |
| `u32CapChange` | 未换算成整百分比的累计容量变化。 |
| `u32IntegrateRemainderMs` | 安时积分余数，避免 200ms 小步截断损失。 |
| `u32Cycle_times` | 内部循环次数，单位 `cycle * 100`。 |
| `u32LearnPassedAs10` | 两个可信锚点之间累计通过电量。 |
| `u16LearnAnchorSoc` | 当前学习锚点，0 或 100。 |
| `u16LearnState` | 学习状态：无、满电锚点、空电锚点。 |
| `u16MaxErrorPercent` | 粗略可信误差，默认 100，学习后 5。 |
| `u16LearnFlags` | 学习标志，`SOC_LEARN_FLAG_LEARNED` 表示已有学习结果。 |
| `u8SOC_Now` | 内部 SOC，0 到 100。 |
| `u8DSG_SOC_Int` | 放电 SOC 百分比累加器，用于循环次数统计。 |

### 5.3 运行上下文 `g_soc_runtime`

用于保存非持久化状态：显示 SOC、当前模式、当前积分方向、进入静置计时、满电确认计时、端点修正计时、静置电压稳定计时、在线 OCV 稳定计时、重载放电后的 holdoff 计时、已应用静置桶等。

这些状态掉电不保存，重启后依赖 Flash 快照恢复长期状态，再重新建立运行上下文。

## 6. 单位换算

| 数据 | 单位 | 说明 |
| --- | --- | --- |
| `g_stCellInfoReport.u16Ichg/u16IDischg` | `A * 10` | 10 表示 1.0A。 |
| `OtherElement.u16Soc_Ah` | `10 * Ah` | 270 表示 27.0Ah。 |
| 内部容量 `u32Cap*` | `As * 10` | 1A 持续 1s 记为 10。 |
| 对外容量 `u16Capacity*` | `Ah * 100` | 2700 表示 27.00Ah。 |
| 内部循环 `u32Cycle_times` | `cycle * 100` | 输出前除以 100。 |

安时积分核心换算：

```c
integrate_acc_ms = current_A10 * 200ms + remainder;
delta_as10 = integrate_acc_ms / 1000;
remainder = integrate_acc_ms % 1000;
```

示例：1.0A 电流在 200ms 内通过 0.2As，内部值为 `0.2 * 10 = 2`，正好对应 `10 * 200 / 1000 = 2`。

对外容量换算：

```c
capacity_ah100 = (cap_as10 + 180) / 360;
```

这是 `As * 10 -> Ah * 100` 的四舍五入换算。

## 7. 启动流程

主程序启动顺序中，SOC 相关路径是：

1. `StorageFlash_PrintBootCheck()` 打印后 64K 存储状态。
2. `InitE2PROM()` 加载默认参数，再从 Flash RW 参数区加载现场参数。
3. `UpgradeParamPolicy_ApplyOnce()` 根据升级策略版本决定是否重置 SOC 配置和快照。
4. `InitData_SOC()` 在 `InitE2PROM()` 之后调用。
5. 主循环中每轮调用 `App_AFEGet()` 和 `App_SOC()`，SOC 实际算法受 200ms 标志和 AFE 新样本序号控制。

`InitData_SOC()` 内部流程：

1. `SOC_LoadConfigData()` 从 `OtherElement` 复制 SOC 基础配置到 `SOC_Enhance_Element`。
2. 将 `SOC_Table_Set[42]` 复制到 `SOC_Enhance_Element.SOC_Table_CanSet`。
3. `soc_param_lib_init()` 清空内部计算状态。
4. `SOC_LoadFactoryRuntimeConfig()` 生成出厂容量和初始循环次数。
5. `SOC_DealEEPROM_Data(EEPROM_DATA_READ)` 从 Flash SOC journal 读取快照。
6. 快照无效时 `SOC_LoadDefaultSnapshot()`，默认 SOC 为 60%，容量按 60% 计算，并尝试写回 Flash。
7. 设置 `u16_SOC_InitOver = 1`，重置运行上下文，强制对外显示跟随内部 SOC。

## 8. 周期执行流程

主循环中 `App_SOC()` 的执行条件：

1. 只有 `g_st_SysTimeFlag.bits.b1Sys200msFlag == 1` 才继续。
2. 使用 `g_u32AfeCurrentSampleSeq` 判断是否有新的 AFE 电流样本。
3. 有新样本时：
   - 复制 `VCellMax/VCellMin/Ichg/IDischg` 到 `SOC_Enhance_Element`。
   - 调用 `SOC_IntEnhance_Ctrl()` 执行完整算法。
4. 没有新样本时：
   - 只调用 `SOC_PublishReportData()` 重新发布当前输出。

`SOC_IntEnhance_Ctrl()` 的固定顺序如下：

1. `SOC_RefreshData_Monitor()` 处理上位机或参数修改触发的 refresh flag。
2. `SOC_GetCurrentDirection()` 判断 CHG/DSG/IDLE/RELAX。
3. `SOC_TrackOnlineOcvRecoveryHoldoff()` 跟踪重载放电后的在线 OCV 禁止窗口。
4. `SOC_RunCoulombCounter()` 做安时积分和端点渐进修正。
5. `SOC_ApplyVoltageCalibration()` 做可信满电确认。
6. `SOC_ApplyOnlineOcvGuard()` 做轻载在线 OCV 小步融合。
7. `SOC_UpdateRestMonitor()` 做运行态静置 OCV 补偿。
8. `SOC_ApplyWeakCellGuard()` 做弱单体低压保护下修。
9. `SOC_EEPROM_Deal_Monitor()` 检查是否需要保存 Flash 快照。
10. `SOC_SyncOutputData(0)` 平滑显示并发布到 `g_stCellInfoReport`。

这个顺序很重要：安时积分先给出主估算，随后电压类逻辑只在可信条件下修正，最后统一平滑发布和持久化。

## 9. 方向与模式判断

电流阈值 `SOC_CURRENT_ENTER_A10 = 4`，即 0.4A。

| 条件 | 方向 |
| --- | --- |
| `Ichg >= 0.4A` 且 `Ichg >= Idsg` | 充电 `CHG` |
| 否则 `Idsg >= 0.4A` | 放电 `DSG` |
| 其他 | 空闲 `IDLE` |

空闲并不会立刻变为静置模式。连续空闲达到 `SOC_MODE_RELAX_ENTRY_SECONDS = 5s` 后，`g_soc_runtime.u8Mode` 才进入 `RELAX`。一旦重新检测到充电或放电，会重置静置监控。

## 10. 安时积分逻辑

`SOC_ApplyCapacityDelta()` 是安时积分核心。

充电时：

1. 按 200ms 积分得到 `delta_as10`。
2. `u32CapNow += delta_as10`，超过 `cap_base` 时饱和到满容量。
3. 通过 `u32CapChange * 100 / cap_base` 换算整百分比。
4. SOC 上升时最多升到 99%，不会仅靠积分变成 100%。
5. 100% 必须由可信满电确认建立。

放电时：

1. `u32CapNow -= delta_as10`，低于 0 时饱和为 0。
2. 按整百分比降低内部 SOC。
3. `u32CapNow == 0` 时 SOC 置 0，并调用 `SOC_OnTrustedEmptyAnchor()` 建立空电锚点。
4. 放电 SOC 变化会进入 `SOC_AddDischargeCyclePercent()`，累计 80% 放电量计 1 次循环。

积分方向变化时，`SOC_SelectIntegrateDirection()` 会清空 `u32CapChange`、积分余数、端点计时，避免前一方向的残余量污染当前方向。

## 11. 端点修正与满电确认

### 11.1 端点渐进修正

`SOC_ApplyTerminalCorrection()` 在充电或放电端点附近小步修正。

充电端：

| 条件 | 计时 | 动作 |
| --- | ---: | --- |
| `VCellMax >= V100 + 50mV` 且 SOC < 100 | 2s | SOC +1，最高到 99。 |
| `VCellMax >= V100` 且 SOC < 100 | SOC > 95 时 8s，否则 4s | SOC +1，最高到 99。 |
| `VCellMax >= V100 - 100mV` 且 `< V100` 且 SOC < 95 | 10s | SOC +1。 |

放电端：

| 条件 | 计时 | 动作 |
| --- | ---: | --- |
| `VCellMin <= V0 - 50mV` 且 SOC > 0 | 2s | SOC -1。 |
| `VCellMin <= V0` 且 SOC > 0 | SOC < 5 时 8s，否则 4s | SOC -1。 |
| `VCellMin <= V0 + 100mV` 且 `> V0` 且 SOC > 5 | 10s | SOC -1。 |

### 11.2 可信满电确认

`SOC_ApplyVoltageCalibration()` 只有在充电模式且 `SOC_IsCalibrationAllowed()` 通过时才工作。当前三元锂默认配置下，满电确认条件是：

1. `VCellMax >= V100`，默认 `>= 4180mV`。
2. `VCellMin >= max(V100 - 80mV, 4000mV)`，默认 `>= 4100mV`。
3. 单体压差 `<= 120mV`。
4. 充电电流非 0 且 `Ichg <= C/20`。默认 27Ah 下约 `<= 1.3A`。
5. 条件持续 `SOC_FULL_CONFIRM_SECONDS = 60s`。

满足后：

1. 内部 SOC 置 100。
2. 调用 `SOC_OnTrustedFullAnchor()` 建立满电锚点。
3. 可能触发容量学习。

## 12. 电压类校准门控

`SOC_IsCalibrationAllowed()` 由三部分组成：

1. `SOC_IsCalibrationVoltageValid()`：电压合法且压差在配置范围内。
2. `SOC_HasBlockingProtectionFault()`：当配置开启时，三级保护故障会阻塞校准。
3. `SOC_HasBlockingSystemFault()`：当配置开启时，AFE/ADC/CBC/温度系统故障会阻塞校准。

当前配置中保护故障和系统故障阻塞开关均为 0，因此主要由电压合法性和压差决定。电压合法性包括：

1. `VCellMin` 和 `VCellMax` 都不低于 2000mV。
2. `VCellMin` 和 `VCellMax` 都不高于 5000mV。
3. `VCellMax >= VCellMin`。
4. 校准压差不超过 1000mV。

注意：弱单体保护只要求基础电压有效，不要求完整 `SOC_IsCalibrationAllowed()`。这是安全下修逻辑，不是普通 OCV 校准。

## 13. 在线 OCV 融合

`SOC_ApplyOnlineOcvGuard()` 目标是轻载运行中慢速收敛，而不是用端电压实时覆盖 SOC。

启用条件：

1. `PROJECT_CFG_SOC_ONLINE_OCV_GUARD_ENABLE = 1`。
2. SOC 已初始化。
3. 校准门控通过。
4. 当前方向为充电或放电。
5. 当前电流属于轻载：`0.4A <= current <= C/10`。默认上限约 2.7A。
6. 不在重载放电后的 holdoff 窗口。
7. OCV 查表目标在可信范围 5% 到 95%。
8. 若为 LFP 表，20% 到 90% 平台区目标不可信，不参与在线 OCV。

方向约束：

| 当前方向 | 允许修正 |
| --- | --- |
| 充电 | 只允许向上修正，且目标必须大于当前 SOC 3% 以上。 |
| 放电 | 只允许向下修正，且当前 SOC 必须高于目标 3% 以上。 |

稳定和速率限制：

1. 电压必须在 8mV 窗口内稳定 20s。
2. 偏差条件还要持续 30s。
3. 每次只向目标移动 1%。
4. 放电电流达到 `C/3` 后启动 180s holdoff，期间在线 OCV 被清零等待。

## 14. 静置与 RTC 休眠补偿

### 14.1 运行态静置补偿

`SOC_UpdateRestMonitor()` 在 `RELAX` 模式下工作：

1. 只有无有效充放电电流并进入 `RELAX` 后才计时。
2. 电压稳定窗口为 3mV，稳定时间 30s。
3. 根据静置时长分桶，只在新桶首次进入时修正一次。

静置桶：

| 静置时间 | 桶 | 最大下修步长 |
| --- | ---: | ---: |
| `< 10min` | 0 | 不修正 |
| `10min - 30min` | 1 | 1% |
| `30min - 60min` | 2 | 1% |
| `60min - 6h` | 3 | 2% |
| `>= 6h` | 4 | 3% |

上修更保守：只有 OCV 目标高于当前 SOC、当前 SOC 已经 `>= 90%` 且桶 `>= 3` 时才允许上修，最大 1% 或 2%。

LFP 表在平台区下修会被限制为最多 1%，避免平台区电压误差造成大幅跳变。

### 14.2 RTC 休眠补偿

`rtc_sleep.c` 中 `update_rtc_soc()` 在 RTC 唤醒时调用：

```c
SOC_ApplyRtcRelaxationCompensation(rest_seconds, VCellMin, VCellMax);
```

它复用静置补偿和弱单体保护逻辑，随后保存快照并强制显示 SOC 跟随内部 SOC。该路径适合休眠期间主循环没有连续 200ms 采样的场景。

## 15. 弱单体保护

`SOC_ApplyWeakCellGuard()` 用于非充电场景下，最低单体接近 `V0` 时强制下修 SOC。

触发前提：

1. 当前不是充电。
2. 电压基础合法。
3. `VCellMin <= V0 + 120mV`。

分段策略：

| 最低单体条件 | 目标上限 | 最大步长 |
| --- | ---: | ---: |
| `VCellMin <= V0` | 0% | 5% |
| `VCellMin <= V0 + 30mV` | 2% | 2% |
| `VCellMin <= V0 + 60mV` | 4% | 1% |
| `VCellMin <= V0 + 90mV` | 6% | 1% |
| `VCellMin <= V0 + 120mV` | 8% | 1% |

实际目标会取上述上限和 OCV 查表目标的较小值。若压到 0% 且 `VCellMin <= V0`，会建立可信空电锚点。

## 16. 显示 SOC 与对外发布

内部 `u8SOC_Now` 和对外 `u8_SOC` 分离。

`SOC_UpdateDisplaySoc()` 的规则：

1. 首次发布时显示 SOC 直接等于内部 SOC。
2. 正常情况下每 `SOC_DISPLAY_STEP_SECONDS = 1s` 向内部 SOC 移动 1%。
3. 严重低压下修时，显示步进周期变成 1 个 SOC 调度 tick，约 200ms，避免低压时对外显示过慢。
4. 初始化、手动刷新、RTC 补偿会使用 `SOC_SyncOutputData(1)` 强制显示跟随内部 SOC。

`SOC_PublishReportData()` 将结果复制到 `g_stCellInfoReport.SocElement`：

| 输出字段 | 来源 |
| --- | --- |
| `u16Soc` | 平滑后的 `SOC_Enhance_Element.u8_SOC`。 |
| `u16Soh` | `CapFull / CapFactory * 100`，上限 100。 |
| `u16CapacityNow` | `u32CapNow` 转 `Ah * 100`。 |
| `u16CapacityFull` | `u32CapFull` 转 `Ah * 100`。 |
| `u16CapacityFactory` | `u32CapFactory` 转 `Ah * 100`。 |
| `u16Cycle_times` | 内部循环次数除以 100。 |

有两个调试/系统开关会覆盖对外 SOC：

1. `System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed`：对外 SOC 强制 60。
2. `System_OnOFF_Func.bits.b1OnOFF_SOC_Zero`：对外 SOC 强制 0。

这两个开关只覆盖发布值，不等价于直接改内部 `u8SOC_Now`。

## 17. Flash 快照与持久化

SOC 快照结构为 `STORAGE_FLASH_SOC_DATA`，版本 `FLASH_STORAGE_SOC_DATA_VERSION_V2 = 0x0002`。

字段包括：

| 字段 | 含义 |
| --- | --- |
| `u16FormatVersion` | 快照格式版本。 |
| `u16SocNow` | 内部 SOC。 |
| `u16DsgSocInt` | 放电百分比累加器。 |
| `u16MaxErrorPercent` | 可信误差。 |
| `u32CycleTimes` | 内部循环次数，`cycle * 100`。 |
| `u32CapNow` | 当前容量，`As * 10`。 |
| `u32CapFull` | 当前满充容量基准，`As * 10`。 |
| `u32LearnPassedAs10` | 学习区间累计电量。 |
| `u16LearnAnchorSoc` | 当前学习锚点。 |
| `u16LearnState` | 当前学习状态。 |
| `u16Flags` | 学习标志。 |

Flash 地址：

| 区域 | 地址 |
| --- | --- |
| SOC slot A | `0x0801E000` |
| SOC slot B | `0x0801E800` |

`Flash.c` 使用 journal 记录：

1. 每条记录有 magic、version、length、sequence、CRC。
2. 读取时选 sequence 最新的有效记录。
3. 写入时优先在当前页追加；页满或异常时切换/擦除目标页。
4. 写后立即读回校验。
5. 兼容旧 V1 快照，旧快照只有 SOC、放电累加、循环次数，读取后升级成 V2 默认字段。

`SOC_PersistSnapshotIfChanged()` 的触发条件不是每 200ms 都保存，而是长期状态发生变化才保存：

1. SOC 变化。
2. 放电累加器变化。
3. 循环次数变化。
4. `CapFull` 变化。
5. 学习锚点、学习状态、可信误差、学习标志变化。

注意：`u32CapNow` 和 `u32LearnPassedAs10` 会被保存到快照，但它们本身不单独触发保存；通常要等 SOC 或学习状态变化时一起落盘。这样可以降低 Flash 写入压力。

## 18. 容量学习与 SOH

容量学习由可信满电锚点和空电锚点驱动。

锚点来源：

1. 满电锚点：可信满电确认持续满足后建立。
2. 空电锚点：放电积分到容量 0，或弱单体保护压到 0 且 `VCellMin <= V0`。

学习规则：

1. 从满电锚点开始，只累计放电方向通过电量。
2. 从空电锚点开始，只累计充电方向通过电量。
3. 中途方向反向且已有累计量，会重置学习状态。
4. 首次学习要求 SOC 跨度至少 90%。
5. 已学习后再次学习要求跨度至少 40%。
6. 学得容量必须在出厂容量的 50% 到 110% 之间。
7. 单次 `CapFull` 调整最多为旧容量的 5%。
8. 学习成功后 `u16MaxErrorPercent` 设为 5，`SOC_LEARN_FLAG_LEARNED` 置位。

SOH 发布逻辑：

1. `CapFull >= CapFactory` 时 SOH 固定 100。
2. `CapFull < CapFactory` 时 SOH = `CapFull / CapFactory * 100`。

## 19. 上位机与参数修改入口

### 19.1 SOC 表

上位机写 `RS485_CMD_ADDR_SOC_VOLTAGE1 = 0x2200`，一次必须写满 `E2P_PARA_NUM_SOC_TABLE = 42` 个 word。写入后：

1. 更新 `SOC_Table_Set[42]`。
2. 调用 `InitData_SOC()` 重新加载到 `SOC_Enhance_Element.SOC_Table_CanSet`。

注意：是否实际使用自定义表取决于 `OtherElement.u16Soc_TableSelect == SOC_TABLE_TEST`。当前写表路径只更新 RAM 并重新初始化，未看到同步调用 `EEPROM_SaveRWParametersToFlash()` 或独立 SOC 表保存 API，因此这条上位机写表路径本身不保证跨复位保存。

### 19.2 SOC 基础参数

SOC 基础参数位于 `OtherElement` 的 offset 24 到 27，对应：

| 字段 | 地址起点 |
| --- | --- |
| `u16Soc_Ah` | `RS485_CMD_ADDR_SOC_AH` |
| `u16Soc_Cycle_times` | 下一个 word |
| `u16Soc_V_100` | 下一个 word |
| `u16Soc_V_0` | 下一个 word |

`Sci_ApplyOtherElementSideEffects()` 检测到 offset 24 到 27 被写入后：

1. 调用 `InitData_SOC()`。
2. 设置 `SOC_Enhance_Element.u16_RefreshData_Flag = 2`。

下一次 SOC 算法执行时，flag 2 会：

1. 保留当前内部 SOC。
2. 重新按新容量加载 `u32CapFactory`。
3. 将 `u32CapFull` 重置为 `u32CapFactory`。
4. 清零放电累加器和学习标志。
5. 按保留 SOC 重新计算 `u32CapNow`。

### 19.3 一次性设 SOC

上位机写 `RS485_CMD_ADDR_SET_ONCE_SOC`，值必须 `<= 100`。写入后：

1. `u16_RefreshData_Flag = 3`。
2. `u8_SetSocOnce = 写入值`。
3. 下一次 SOC 算法将内部 SOC 直接设置到目标值，重置学习状态和可信误差。

### 19.4 手动 OCV 刷新

`u16_RefreshData_Flag = 1` 时：

1. 先检查 `SOC_IsCalibrationAllowed()`。
2. 读取当前 `VCellMin` 对应 OCV SOC。
3. 如果当前不是充电，且 OCV 目标高于内部 SOC，则不允许上修，只保持原值。
4. 重置学习状态并应用目标 SOC。

### 19.5 系统开关副作用

上位机打开 `SOC_Fixed` 时设置 refresh flag 1；打开 `SOC_Zero` 时设置 refresh flag 2。同时 `SOC_PublishReportData()` 会在发布阶段把对外 SOC 覆盖为 60 或 0。

## 20. 对外消费者

| 模块 | 使用方式 |
| --- | --- |
| `Sci_Upper.c` | LCD/上位机读实时 SOC；写 SOC 表、基础参数、一次性 SOC。 |
| `Can_HDX.c` | CAN 周期报文发送 SOC、SOH、满电容量、剩余容量、循环次数。 |
| `LedBar.c` | 从 `g_stCellInfoReport.SocElement.u16Soc` 读取显示电量段。 |
| `Fault.c` | 低 SOC 一级/二级/三级故障判断使用 `u16Soc`。 |
| `rtc_sleep.c` | RTC 唤醒后读取补偿后的 `SOC_Enhance_Element.u8_SOC`。 |

低 SOC 故障由 `PRT_E2ROMParas.u16SocUp_First/Second/Third/Rcv/Filter` 控制，实际比较值来自对外发布 SOC。

## 21. 单函数功能速查

### 21.1 `SOC.c`

| 函数 | 功能 |
| --- | --- |
| `SOC_LoadConfigData()` | 从 `OtherElement` 和 `SOC_Table_Set` 加载 SOC 配置到 `SOC_Enhance_Element`。 |
| `InitData_SOC()` | SOC 初始化入口，加载配置、初始化算法、发布一次数据。 |
| `App_SOC()` | 200ms 周期任务入口。有新 AFE 样本时运行完整算法，否则仅发布当前数据。 |

### 21.2 `SocEnhance.c` 对外函数

| 函数 | 功能 |
| --- | --- |
| `SOC_UpdateSampleData()` | 更新本次算法使用的最大/最小单体电压和充/放电电流。 |
| `SOC_PublishReportData()` | 将 `SOC_Enhance_Element` 输出复制到 `g_stCellInfoReport.SocElement`，并处理 SOC fixed/zero 覆盖。 |
| `soc_param_lib_init()` | 清空内部状态、加载容量基准、读取 Flash 快照、标记初始化完成、强制同步输出。 |
| `SOC_ResetStoredSnapshotToDefault()` | 将 Flash SOC 快照重置为默认 60%、默认容量、默认循环次数。 |
| `SOC_IntEnhance_Ctrl()` | SOC 完整周期算法入口。 |
| `SOC_ApplyRtcRelaxationCompensation()` | RTC 唤醒后按休眠时长和当前电压执行静置补偿、弱单体保护、保存快照并同步输出。 |

### 21.3 `SocEnhance.c` 内部函数

| 函数 | 功能 |
| --- | --- |
| `SOC_ResetRuntimeContext()` | 清空运行上下文，并将模式置为 `RELAX`。 |
| `SOC_IsVoltageValid()` | 检查电压上下限和最大/最小顺序。 |
| `SOC_GetCellDeltaMv()` | 计算最大最小单体压差。 |
| `SOC_IsCalibrationVoltageValid()` | 检查电压合法性和校准压差。 |
| `SOC_HasBlockingProtectionFault()` | 根据配置判断三级保护故障是否阻塞校准。 |
| `SOC_HasBlockingSystemFault()` | 根据配置判断 AFE/ADC/CBC/温度故障是否阻塞校准。 |
| `SOC_IsCalibrationAllowed()` | 统一电压类校准门控。 |
| `SOC_GetCapBase()` | 优先返回 `CapFull`，否则返回 `CapFactory`。 |
| `SOC_SelectIntegrateDirection()` | 切换积分方向，并清理方向相关累计量。 |
| `SOC_ApplyCapacityDelta()` | 按电流和 200ms 周期积分容量，更新内部 SOC。 |
| `SOC_ResetLearningState()` | 清空容量学习状态。 |
| `SOC_AddLearnPassed()` | 在可信锚点后累计正确方向通过电量。 |
| `SOC_ApplyLearnedFullCapacity()` | 将学习容量按范围和 5% 步长限制应用到 `CapFull`。 |
| `SOC_OnTrustedFullAnchor()` | 建立满电锚点，必要时根据上一个空电锚点学习容量。 |
| `SOC_OnTrustedEmptyAnchor()` | 建立空电锚点，必要时根据上一个满电锚点学习容量。 |
| `SOC_AddDischargeCyclePercent()` | 放电百分比累计满 80% 时循环次数加 1。 |
| `SOC_SetSocValue()` | 设置内部 SOC，并按容量基准重算 `CapNow`。 |
| `SOC_ApplySocNow()` | 强制应用一个 SOC 值，清理积分累计。 |
| `SOC_ApplySocCorrection()` | 修正内部 SOC，当前实现等同于 `SOC_SetSocValue(..., clear=1)`。 |
| `SOC_StepTowardTarget()` | 按最大步长向目标 SOC 移动。 |
| `SOC_PersistSnapshotIfChanged()` | 检测长期状态变化并写 Flash 快照。 |
| `SOC_GetRestBucket()` | 将静置秒数映射到静置补偿桶。 |
| `SOC_ResetRestMonitor()` | 清空静置补偿计时和电压参考。 |
| `SOC_UpdateRelaxVoltageStable()` | 跟踪静置模式下电压是否稳定。 |
| `SOC_IsRelaxVoltageStable()` | 判断静置电压稳定计时是否满足。 |
| `SOC_ApplyRestCompensation()` | 根据静置时长和 OCV 目标执行小步补偿。 |
| `SOC_UpdateRestMonitor()` | 在 `RELAX` 模式下累计静置时间并调用补偿。 |
| `SOC_ApplyWeakCellGuard()` | 低压弱单体场景下强制下修 SOC。 |
| `SOC_UpdateDisplaySoc()` | 将显示 SOC 平滑追随内部 SOC。 |
| `SOC_CapAs10ToAh100()` | 内部容量 `As * 10` 转对外 `Ah * 100`。 |
| `SOC_SyncOutputData()` | 同步显示 SOC、SOH、容量、循环次数并发布。 |
| `SOC_LoadFactoryRuntimeConfig()` | 按配置容量和循环次数生成内部基准。 |
| `SOC_ResetCalculateState()` | 清空内部计算结构。 |
| `SOC_LoadDefaultSnapshot()` | 创建默认 60% 快照状态。 |
| `SOC_GetSelectedOcvTable()` | 按表选择返回当前 OCV 表指针和长度。 |
| `Get_OpenCircuit_Value()` | 用 `VCellMin` 查 OCV 表得到目标 SOC。 |
| `SOC_GetFullCellConfirmVoltage()` | 计算满电确认要求的最低单体电压。 |
| `SOC_IsFullConfirmCellDeltaValid()` | 检查满电确认压差。 |
| `SOC_GetCurrentLimitA10()` | 按 `C/divider` 计算电流阈值，最小 0.4A。 |
| `SOC_TerminalCounterReady()` | 通用端点计时器。 |
| `SOC_ApplyTerminalCorrection()` | 充/放电端点附近按计时小步修正 SOC。 |
| `SOC_GetMeasuredCurrentDirection()` | 根据当前电流判断瞬时方向。 |
| `SOC_GetCurrentDirection()` | 带 5s 静置滞后的方向/模式判断。 |
| `SOC_RunCoulombCounter()` | 根据方向执行安时积分或进入 idle。 |
| `SOC_ResetOnlineOcvStability()` | 清空在线 OCV 电压稳定参考。 |
| `SOC_ResetOnlineOcvGuard()` | 清空在线 OCV 计时和方向。 |
| `SOC_UpdateOnlineOcvVoltageStable()` | 判断在线 OCV 的电压稳定条件。 |
| `SOC_TrackOnlineOcvRecoveryHoldoff()` | 重载放电后启动/递减在线 OCV holdoff。 |
| `SOC_IsOnlineOcvTargetTrusted()` | 判断 OCV 目标是否处于可信 SOC 区间。 |
| `SOC_IsOnlineOcvLightCurrent()` | 判断当前方向电流是否属于轻载。 |
| `SOC_ApplyOnlineOcvGuard()` | 轻载在线 OCV 小步融合。 |
| `SOC_DealEEPROM_Data()` | 读写 SOC Flash 快照，历史函数名仍含 EEPROM。 |
| `SOC_Update_StartUp()` | 处理 refresh flag 1/2/3。 |
| `SOC_EEPROM_Deal_Monitor()` | 周期调用快照变化检测。 |
| `SOC_RefreshData_Monitor()` | 初始化完成后处理 refresh flag。 |
| `isCHG()` / `isDSG()` | 结合运行模式和瞬时电流判断是否充/放电。 |
| `SOC_GetTaperCurrentA10()` | 计算满电确认 taper 电流，当前为 `C/20`。 |
| `SOC_ApplyVoltageCalibration()` | 满电可信确认，满足后置 100% 并建立满电锚点。 |

## 22. 关键边界与注意点

1. `App_SOC()` 只有在 AFE 样本序号变化时才运行完整算法；如果 `App_AFEGet()` 因串口忙而跳过，SOC 不会积分，只会重新发布旧值。
2. 充电积分最多到 99%，100% 只能由可信满电确认产生。
3. 当前配置下三级保护和系统故障不会阻塞 SOC 电压类校准；如果要更保守，需要把两个 block 配置改为 1。
4. 自定义 SOC 表写入路径当前只更新 RAM；即使 `PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_TABLE = 0`，复位后也不能只依赖该写表命令恢复现场表。
5. 对外 SOC 可能被 `SOC_Fixed` 或 `SOC_Zero` 覆盖，排查 SOC 计算异常时要先确认系统开关状态。
6. Flash 快照降低了重启后 SOC 跳变，但为了减少写入压力，不会因每个 `CapNow` 小变化立即写 Flash。
7. 弱单体保护是安全下修逻辑，可能在非完整校准门控下工作，因此低压异常排查时要重点看 `VCellMin`、`V0` 和电压采样合法性。
8. 当前 `SOC_Enhance_Element.u8_SOC_OCV_Cali` 实际导出的是放电百分比累加器，不是 OCV 校准状态，命名和含义不完全一致。

## 23. 排查建议

如果现场 SOC 异常，建议按以下顺序排查：

1. 看 `g_stCellInfoReport.u16Ichg/u16IDischg` 是否每 200ms 更新，`g_u32AfeCurrentSampleSeq` 是否增长。
2. 看 `VCellMin/VCellMax` 是否在 2000 到 5000mV 且顺序正确。
3. 看 `OtherElement.u16Soc_Ah/u16Soc_V_100/u16Soc_V_0/u16Soc_TableSelect` 是否符合电池型号。
4. 看 `System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed/Zero` 是否覆盖了对外值。
5. 看 Flash 快照是否有效，启动是否回退到默认 60%。
6. 充电不满 100 时，检查 `VCellMax >= V100`、`VCellMin >= V100 - margin`、压差、taper 电流、60s 持续时间。
7. 放电 SOC 快速下修时，检查 `VCellMin` 是否进入 `V0 + 120mV` 的弱单体保护窗口。
8. 轻载运行 SOC 不收敛时，检查在线 OCV 的轻载阈值、重载 holdoff、稳定窗口、目标差值是否满足。
