# SOC 模块逻辑与首次烧录默认值

适用当前配置：`103 + 309/Project/Source/conf/conf.h` 中 `BAT_TYPE = BAT_SLAVE`、`LIFEPO` 已启用。若切到 `BAT_MASTER` 或三元锂，以下容量和电压默认值会随 `DataDeal.h` 的宏变化。

## 1. 首次烧录且后 64K 参数区为空时的值

启动顺序是：

1. `InitE2PROM()` 先把编译期默认参数加载到 RAM。
2. 如果后 64K 的 RW 参数区没有有效数据，则把默认参数写入 `0x0801C400/0x0801CC00`。
3. `InitData_SOC()` 把 `OtherElement` 中的 SOC 配置复制到 `SOC_Enhance_Element`。
4. `soc_param_lib_init()` 读取 SOC 快照；如果 `0x0801E000/0x0801E800` 没有有效快照，则生成默认 SOC 快照并写入 Flash journal。

当前首次默认 SOC 配置：

| 变量 | 首次值 | 说明 |
|---|---:|---|
| `u16_SOC_TableSelect` | `SOC_TABLE_LIFEPO` = 1 | 磷酸铁锂 OCV 表 |
| `u16_SOC_Ah` | 270 | 单位 `10 * Ah`，即 27.0Ah |
| `u16_SOC_CycleT_Ever` | 3 | 初始循环次数 |
| `u16_SOC_CycleT_Limit` | 5000 | SOC 模块内部默认循环寿命 |
| `u16_SOC_100_Vol` | 3600mV | 满电端点电压 |
| `u16_SOC_0_Vol` | 3000mV | 空电端点电压 |
| `u8SOC_Now` | 60% | 没有历史 SOC 快照时的启动 SOC |
| `u8DSG_SOC_Int` | 0 | 放电循环累计百分比 |
| `u32Cycle_times` | 300 | 内部按 `循环次数 * 100` 保存，对外显示 3 |
| `u32CapFactory` | 972000 | `270 * 3600`，内部 As*10 计量 |
| `u32CapFull` | 972000 | 首次默认 SOH 为 100% |
| `u32CapNow` | 583200 | 60% 剩余容量 |
| `u8_SOC` | 60 | 对外 SOC |
| `u8_SOH` | 100 | 对外 SOH |
| `u16_CapacityNow` | 1620 | 单位 `Ah * 100`，即 16.20Ah |
| `u16_CapacityFull` | 2700 | 27.00Ah |
| `u16_CapacityFactory` | 2700 | 27.00Ah |
| `u16_Cycle_times` | 3 | 对外循环次数 |

首次默认 OCV 表来自 `SOC_Table_Default`，当前 21 组点为：

```text
3336/100, 3332/90, 3330/80, 3327/75, 3316/70,
3301/65, 3294/60, 3291/55, 3290/50, 3288/45,
3286/40, 3279/35, 3266/30, 3254/25, 3236/20,
3212/15, 3198/10, 3112/5, 2526/0, 1000/0, 1000/0
```

电压、电流采样字段 `u16_VCellMax/u16_VCellMin/u16_Ichg/u16_Idsg` 上电时为全局零初始化值，主循环 200ms 周期进入 `App_SOC()` 后从 `g_stCellInfoReport` 刷新。

## 2. 后续烧录能否修改

普通 APP 烧录通常不会擦除后 64K 参数区，所以已经有效的 Flash 参数会优先于新固件默认值：

| 数据 | 存储位置 | 后续烧录默认是否覆盖 |
|---|---|---|
| SOC 配置：容量、循环次数、0/100 电压、表选择 | RW 参数区 `0x0801C400/0x0801CC00` | 不自动覆盖 |
| SOC 运行快照：当前 SOC、放电累计、循环次数 | SOC journal `0x0801E000/0x0801E800` | 不自动覆盖 |
| SOC 默认 OCV 表 | 当前只在 RAM 中加载 | 新固件默认表会在每次启动加载；上位机写表当前不跨重启保存 |

需要让后续固件主动改现场参数时，有四种方式：

1. 上位机写 `RS485_CMD_ADDR_SOC_AH` 起始的 SOC 配置参数，成功后会走 `EEPROM_SaveRWParametersToFlash()` 落盘。
2. 修改 `UpgradeParamPolicy.h`：打开 `UPGRADE_PARAM_RESET_SOC_CONFIG`，并递增 `UPGRADE_PARAM_POLICY_VERSION`，让升级包一次性覆盖现场 SOC 配置。
3. 修改 `UpgradeParamPolicy.h`：打开 `UPGRADE_PARAM_RESET_SOC_SNAPSHOT`，并递增 `UPGRADE_PARAM_POLICY_VERSION`，让升级包一次性把历史 SOC 快照写回默认启动快照。
4. 擦除后 64K 参数区或用维护命令重置参数，让固件重新按编译期默认值初始化。

注意：`UPGRADE_PARAM_RESET_SOC_CONFIG` 只覆盖 SOC 配置，不会自动清空 SOC journal 中的历史 SOC 快照；`UPGRADE_PARAM_RESET_SOC_SNAPSHOT` 只重写 SOC journal，默认写入 SOC=60%、放电累计=0、循环次数=当前 SOC 配置循环次数。需要升级后基础参数和历史快照一起回到默认值时，两个开关要同时打开。

## 3. 运行逻辑

`App_AFEGet()` 每 200ms 成功刷新一次 AFE 电流后递增 `g_u32AfeCurrentSampleSeq`。`App_SOC()` 同样挂在 200ms 时基上，但只消费新的 AFE 样本，避免通信忙导致 AFE 未刷新时重复积分旧电流。

当前 SOC 数据流：

1. `App_AFEGet()` 刷新电压、电流，并递增 AFE 样本序号。
2. `App_SOC()` 检查样本序号，有新样本才调用 `SOC_UpdateSampleData()`。
3. `SOC_IntEnhance_Ctrl()` 按“命令处理 → 电流方向判断 → 200ms 安时积分 → 端点电压校准 → 静置 OCV 修正 → 弱单体低压保护 → 快照持久化 → 输出同步”顺序执行。
4. 上位机刷新命令仍保留：`1` 为 OCV 刷新，`2` 为容量/循环初始化，`3` 为设置 SOC 一次。
5. SOC、放电累计或循环次数变化时写入 SOC journal。
6. 对外 SOC 每个 200ms 样本最多向内部 SOC 靠近 1%，手动刷新和 RTC 休眠补偿会强制同步。

## 4. SOC 校准策略

当前 SOC 不是单纯按电压查表，也不是单纯安时积分，而是以下几类策略叠加：

1. 安时积分作为主路径，按充/放电电流每 200ms 累加一次容量变化，并保存小数余量。
2. 充电末端按单体最高电压逐步拉高 SOC，满足满电条件时可直接校准到 100%。
3. 放电末端按单体最低电压逐步拉低 SOC。
4. 弱单体低压保护会限制 SOC 上限，避免某个单体已经接近空电时 SOC 仍显示很高。
5. 静置或 RTC 休眠唤醒后，用 OCV 表做有限幅度修正。
6. 上位机可触发 OCV 刷新、容量重置、设置一次 SOC。

### 4.1 内部 SOC 与对外 SOC

代码里需要区分三层 SOC：

| 层级 | 变量 | 说明 |
|---|---|---|
| 内部估算 SOC | `SOC_Calculate_Element.u8SOC_Now` | 算法真实状态，安时积分、端点校准、弱单体保护都会直接修改它 |
| 内部剩余容量 | `SOC_Calculate_Element.u32CapNow` | 单位 `As * 10`，每次 `SOC_SetSocValue()` 都会按 SOC 百分比重新计算 |
| 对外发布 SOC | `SOC_Enhance_Element.u8_SOC` / `g_stCellInfoReport.SocElement.u16Soc` | 默认每个有效 200ms 样本向内部 SOC 靠近 1%，用于显示和通信 |

因此“校准到 0”要分两种情况看：

- 内部容量/SOC 可以被直接或快速校准到目标值。
- 对外显示 SOC 默认是平滑变化的，每个有效 200ms 样本最多变化 1%，避免数码管、CAN、RS485 读数突然跳变。

`SOC_ApplyRtcRelaxationCompensation()`、手动 OCV 刷新、容量重置、设置一次 SOC 会强制对外 SOC 跟随内部值；正常 200ms 主循环中的积分和低压保护则经显示平滑路径发布。

### 4.2 安时积分主路径

当前已经取消原来的 `SOC_CALI_STATE_TRANSFER / CONT_CHG / CONT_DSG` 状态机，直接按最新 AFE 样本判断方向：

| 条件 | 行为 |
|---|---|
| `u16_Ichg >= SOC_VIRTUAL_CURRENT_CHG` 且不小于放电电流 | 本次 200ms 样本按充电积分 |
| `u16_Idsg >= SOC_VIRTUAL_CURRENT_DSG` | 本次 200ms 样本按放电积分 |
| 两者都低于阈值 | 空闲，不积分，静置计时可继续累计 |

每个 200ms 样本的容量增量为 `current(A*10) * 200 / 1000`，内部单位仍为 `As * 10`。不能整除的余量保存在 `u32IntegrateRemainderMs`，所以 0.2A 这类小电流不会因为 200ms 周期被直接截断为 0。

低容量边界处理在 `SOC_ApplyCapacityDelta()` 中完成：

| 场景 | 内部处理 |
|---|---|
| 充电后 `u32CapNow >= cap_base` | `u32CapNow = cap_base`，`u8SOC_Now = 100` |
| 放电后 `u32CapNow <= 0` | `u32CapNow = 0`，`u8SOC_Now = 0` |
| 未到边界但累计容量达到 1% | SOC 按累计百分比增减，余量保留到下一次积分 |

结论：如果是安时积分把容量放空，内部剩余容量会直接归 0，内部 SOC 也会直接归 0；但对外 SOC 仍会按显示平滑策略逐步降到 0。

### 4.3 充电末端校准

充电末端校准在 `CorrectionTerminal_CV(CurCHG)` 和 `SOC_ApplyVoltageCalibration()` 中完成。

| 条件 | 内部 SOC 修正 |
|---|---|
| `VCellMax >= V100 - 100mV` 且 `< V100`，当前 SOC `< 95` | 每 10s 增加 1% |
| `VCellMax >= V100`，当前 SOC `> 95` 且 `< 100` | 每 8s 增加 1% |
| `VCellMax >= V100`，当前 SOC `<= 95` | 每 4s 增加 1% |
| `VCellMax >= V100 + 50mV`，当前 SOC `< 100` | 每 2s 增加 1% |
| 正在充电，`VCellMax >= V100` 且 `VCellMin >= total_soc100` | 内部 SOC 直接校准到 100% |

`total_soc100` 是源码内按电芯体系固定的辅助条件：`LIFEPO` 为 `3300mV`，`TERNARYLI` 为 `4000mV`。它用于避免只有最高单体达到满电阈值时就把整包 SOC 直接拉到 100%。

### 4.4 放电末端与低压校准

放电低压校准有两条路径：端点 CV 修正和弱单体保护。

端点 CV 修正在持续放电积分路径内执行，按 200ms tick 计数，但阈值仍按秒级窗口配置：

| 条件 | 内部 SOC 修正 |
|---|---|
| `VCellMin <= V0 + 100mV` 且 `> V0`，当前 SOC `> 5` | 每 10s 降低 1% |
| `VCellMin <= V0`，当前 SOC `>= 5` | 每 4s 降低 1% |
| `VCellMin <= V0`，当前 SOC `< 5` | 每 8s 降低 1% |
| `VCellMin <= V0 - 50mV`，当前 SOC `> 0` | 每 2s 额外降低 1% |

弱单体保护在每次 `SOC_IntEnhance_Ctrl()` 中都会检查，速度更快。实际 SOC 上限取 `Get_OpenCircuit_Value()` 查表结果和下表窗口上限的较小值：

| `VCellMin` 条件 | SOC 上限 |
|---|---:|
| `<= V0` | 0% |
| `<= V0 + 30mV` | 2% |
| `<= V0 + 60mV` | 4% |
| `<= V0 + 90mV` | 6% |
| `<= V0 + 120mV` | 8% |

弱单体保护每次最多把内部 SOC 降低 1%。在 200ms 调度正常时，内部 SOC 最快约每 200ms 降 1%，直到不高于上表限制值。

因此低压时的实际表现是：

1. 如果安时积分已经把容量耗尽，内部 `u32CapNow` 和 `u8SOC_Now` 会直接归 0。
2. 如果是电压先触发低压保护，内部 SOC 会按端点修正和弱单体保护逐步降低；`SOC_ApplySocCorrection()` 会同步按新 SOC 重新计算 `u32CapNow`。
3. 对外 SOC 通常不会立即跳到内部目标，而是每个有效 200ms 样本最多下降 1%，除非走手动刷新或 RTC 休眠补偿这类强制同步路径。

### 4.5 静置 OCV 与 RTC 休眠补偿

运行态静置时，必须同时满足无充电、无放电、单体电压有效，才会累计静置时间。静置时间分桶如下：

| 静置时间 | bucket | 最大修正步长 |
|---:|---:|---:|
| `< 10min` | 0 | 不修正 |
| `10min ~ 30min` | 1 | 向下最多 1% |
| `30min ~ 1h` | 2 | 向下最多 1% |
| `1h ~ 6h` | 3 | 向下最多 2%；若当前 SOC `>= 90`，可向上最多 1% |
| `>= 6h` | 4 | 向下最多 3%；若当前 SOC `>= 90`，可向上最多 2% |

静置 OCV 默认偏保守：中低 SOC 区间只允许向下修正，不允许静置后把 SOC 往上拉。RTC 休眠唤醒时调用 `SOC_ApplyRtcRelaxationCompensation()`，使用休眠秒数和唤醒后的单体电压走同一套 OCV 修正，并强制同步对外 SOC。

### 4.6 手动校准入口

| 入口 | 标志/函数 | 行为 |
|---|---|---|
| OCV 刷新 | `u16_RefreshData_Flag = 1` | 按当前 `VCellMin` 查 OCV 表；非充电状态下只允许降低 SOC，不允许抬高 SOC |
| 容量/循环初始化 | `u16_RefreshData_Flag = 2` | 清空放电循环累计，重新加载容量配置，`u32CapFull = u32CapFactory` |
| 设置一次 SOC | `u16_RefreshData_Flag = 3` + `u8_SetSocOnce` | 把内部 SOC 设置为指定 `0~100`，并按容量基准重算 `u32CapNow` |
| SOC 固定 | `b1OnOFF_SOC_Fixed` | 对外发布 SOC 强制为 60%，不改变内部积分状态 |
| SOC 清零显示 | `b1OnOFF_SOC_Zero` | 对外发布 SOC 强制为 0%，不等价于清空内部容量快照 |

## 5. 可调整参数

### 5.1 上位机/RW 参数可调整项

| 参数 | 当前默认值 | 作用 | 生效方式与注意点 |
|---|---:|---|---|
| `OtherElement.u16Soc_Ah` | `270` | 出厂容量，单位 `10 * Ah`，当前为 27.0Ah | 通过 `RS485_CMD_ADDR_SOC_AH` 写入后保存到 RW 参数区；影响积分分母和容量显示 |
| `OtherElement.u16Soc_Cycle_times` | `3` | 初始循环次数 | 通过 `RS485_CMD_ADDR_SOC_CYCLE_TIME` 写入；历史 SOC 快照存在时，运行循环次数优先来自 SOC journal |
| `OtherElement.u16Soc_V_100` | `3600mV` | 满电端点电压 `V100` | 当前枚举名为 `RS485_CMD_ADDR_SOC_RES1`，实际含义是 SOC 100% 电压 |
| `OtherElement.u16Soc_V_0` | `3000mV` | 空电端点电压 `V0` | 当前枚举名为 `RS485_CMD_ADDR_SOC_RES2`，实际含义是 SOC 0% 电压 |
| `OtherElement.u16Soc_TableSelect` | `SOC_TABLE_LIFEPO` | OCV 表选择 | 当前位于 `OtherElement` offset 12；枚举名仍有历史残留，修改通信地址文档时需同步澄清 |
| `SOC_Table_Set[42]` | `SOC_Table_Default` | 自定义 OCV 表，21 组 `电压/SOC` | 只有选择 `SOC_TABLE_TEST` 时用于查表；当前只在 RAM 中生效，断电重启后恢复默认 |
| `u8_SetSocOnce` | 无固定默认 | 一次性设置 SOC | 上位机写 `0~100` 后触发 `u16_RefreshData_Flag = 3`，会同步重算容量并持久化快照 |

### 5.2 源码常量可调整项

| 参数 | 当前值 | 作用 | 修改风险 |
|---|---:|---|---|
| `SOC_DEFAULT_STARTUP_PERCENT` | `60%` | 无有效 SOC 快照时的默认启动 SOC | 首次烧录或 SOC journal 无效时生效 |
| `SOC_VIRTUAL_CURRENT_CHG` | `2` | 充电积分阈值，单位 `A * 10` | `DataDeal.c` 当前会把 `<= 3` 的电流清零，实际有效阈值受采样清零逻辑影响 |
| `SOC_VIRTUAL_CURRENT_DSG` | `2` | 放电积分阈值，单位 `A * 10` | 同上 |
| `SOC_TICKS_PER_SECOND` | `5` | SOC 内部把 200ms tick 换算为秒 | 若 `App_SOC()` 调度周期变化，必须同步修改 |
| `SOC_REST_BUCKET_1_SECONDS` | `600` | 静置 OCV 第一档 | 改小会增加静置修正频率，可能放大 OCV 表误差 |
| `SOC_REST_BUCKET_2_SECONDS` | `1800` | 静置 OCV 第二档 | 同上 |
| `SOC_REST_BUCKET_3_SECONDS` | `3600` | 静置 OCV 第三档 | 同上 |
| `SOC_REST_BUCKET_4_SECONDS` | `21600` | 静置 OCV 第四档 | 同上 |
| `SOC_WEAK_CELL_GUARD_WINDOW_MV` | `120mV` | 弱单体保护总窗口 | 改大后 SOC 会更早被低压单体压低 |
| `SOC_WEAK_CELL_CRITICAL_WINDOW_MV` | `30mV` | 弱单体临界窗口 | 当前还配合 `60mV/90mV/120mV` 分档使用 |
| `SOC_CYCLE_PERCENT_PER_COUNT` | `80%` | 放电累计多少百分比计 1 次循环 | 改动会影响循环次数增长速度 |
| 充电端点计数 | `10s/8s/4s/2s` | 充电末端逐步上修速度 | 位于 `CorrectionTerminal_CV(CurCHG)`，建议结合实测曲线调整 |
| 放电端点计数 | `10s/8s/4s/2s` | 放电末端逐步下修速度 | 位于 `CorrectionTerminal_CV(CurDSG)`，建议结合低压保护点调整 |

### 5.3 不建议当作普通参数调整的项

| 项 | 原因 |
|---|---|
| `u32CapFull` | 当前未持久化真实学习值，启动时会回到 `u32CapFactory`；直接改运行变量不能形成 SOH 闭环 |
| `SOC_Calculate_Element.u32CapChange` | 这是 1% 积分余量，方向切换时会清零；不应作为外部配置 |
| `g_soc_runtime.u8DisplaySoc` | 只是对外显示平滑值，改它不会改变内部容量和 SOC 快照 |
| `b1OnOFF_SOC_Fixed` / `b1OnOFF_SOC_Zero` | 主要用于调试/生产显示覆盖，不应作为真实 SOC 校准手段 |

## 6. 本次策略与架构优化

本次整理后，SOC 初始化集中为三步：加载容量配置、清零计算状态、读取或创建 SOC 快照；删除了没有调用路径或没有计算作用的旧字段和旧初始化函数。

本次优化把 `SOC.c` 收敛为应用层调度入口：只负责从 `OtherElement` 加载 SOC 配置、在 200ms 周期把系统采样传入 SOC 模块、触发 SOC 控制。SOC 输入刷新和对外报告结构写入统一放到 `SocEnhance.c` 的 `SOC_UpdateSampleData()`、`SOC_PublishReportData()`，这样固定 SOC、SOC 置零、容量/SOH/循环次数这些对外发布规则只有一个出口。

积分策略改为以当前可用容量基准计算：

| 项目 | 优化前 | 优化后 |
|---|---|---|
| SOC 百分比积分分母 | 固定使用 `u32CapFactory` | 优先使用 `u32CapFull`，无有效满充容量时回退 `u32CapFactory` |
| 充/放电方向切换 | 共用 `u32CapChange`，上一方向未满 1% 的余量会带到反方向 | 增加 `u8IntegrateDirection`，方向改变时清空积分余量 |
| 剩余容量边界 | 充电按出厂容量截断 | 按当前容量基准截断，避免 SOH 容量下降后 SOC 仍按出厂容量漂移 |
| 循环次数 | 放电积分到 80% 增加 1 次 | 保留 80% 规则，改由本轮放电导致的 SOC 下降量累计 |

持久化策略也做了收敛：`SOC_DealEEPROM_Data()` 改为返回保存结果，`SOC_PersistSnapshotIfChanged()` 只有在 Flash journal 保存成功后才更新备份值。若首次启动创建默认 SOC 快照但写 Flash 失败，会把备份 SOC 置为非法哨兵，后续 200ms 监控仍会继续尝试保存，避免一次写入失败后永久不重试。

仍需注意的边界：

1. `SOC_Table_Set` 仍是 RAM 表，上位机写自定义 OCV 表后不会跨重启保存。
2. 当前 `u32CapFull` 启动时仍等于出厂容量，后续如果要做真实 SOH 学习，需要把满充容量学习值纳入 SOC journal 或独立参数区。
3. `SOC_ApplyRtcRelaxationCompensation()` 只在 RTC 休眠唤醒后使用休眠秒数做静置 OCV 补偿，不替代运行态 200ms 积分。
