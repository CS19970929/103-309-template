# SOC 模块逻辑与首次烧录默认值

适用当前配置：`103 + 309/Project/Source/conf/Project_Config.h` 中 `PROJECT_CFG_BAT_TYPE = 1`、`PROJECT_CFG_BAT_CHEMISTRY = 0`，即 `BAT_SLAVE` + `TERNARYLI`，当前产品默认按三元锂、10 串、27Ah 处理。若切到 `BAT_MASTER`、`LIFEPO` 或修改容量参数，容量、电压端点和 OCV 表会随配置变化。

## 1. 首次烧录默认值

启动顺序：

1. `InitE2PROM()` 把编译期默认参数加载到 RAM。
2. 如果后 64K 的 RW 参数区无有效数据，则把默认参数写入 `0x0801C400/0x0801CC00`。
3. `InitData_SOC()` 把 `OtherElement` 中的 SOC 配置复制到 `SOC_Enhance_Element`。
4. `soc_param_lib_init()` 读取 SOC 快照；如果 `0x0801E000/0x0801E800` 没有有效快照，则创建默认 V2 快照并写入 Flash journal。

当前首次默认 SOC 配置：

| 变量 | 首次值 | 说明 |
| --- | ---: | --- |
| `u16_SOC_TableSelect` | `SOC_TABLE_TERNARYLI` = 2 | 三元锂 OCV 表 |
| `u16_SOC_Ah` | 270 | 单位 `10 * Ah`，即 27.0Ah |
| `u16_SOC_CycleT_Ever` | 3 | 初始循环次数 |
| `u16_SOC_CycleT_Limit` | 5000 | SOC 模块内部默认循环寿命 |
| `u16_SOC_100_Vol` | 4180mV | 满电端点电压 |
| `u16_SOC_0_Vol` | 3000mV | 空电端点电压 |
| `u8SOC_Now` | 60% | 没有历史 SOC 快照时的启动 SOC |
| `u8DSG_SOC_Int` | 0 | 放电循环累计百分比 |
| `u32Cycle_times` | 300 | 内部按 `循环次数 * 100` 保存，对外显示 3 |
| `u32CapFactory` | 972000 | `270 * 3600`，内部单位 `As * 10` |
| `u32CapFull` | 972000 | 首次默认 `FCC/SOH` 为 100% |
| `u32CapNow` | 583200 | 60% 剩余容量 |
| `u16MaxErrorPercent` | 100 | 默认可信度较低，等待端点/学习收敛 |
| `u8_SOC` | 60 | 对外 SOC |
| `u8_SOH` | 100 | 对外 SOH |
| `u16_CapacityNow` | 1620 | 单位 `Ah * 100`，即 16.20Ah |
| `u16_CapacityFull` | 2700 | 27.00Ah |
| `u16_CapacityFactory` | 2700 | 27.00Ah |
| `u16_Cycle_times` | 3 | 对外循环次数 |

默认 SOC 表由 `OtherElement.u16Soc_TableSelect` 选择。当前 `TERNARYLI` 默认使用 `SocTable_TernaryLi`，21 组点为：

```text
4126/100, 4066/95, 4011/90, 3955/85, 3888/80,
3837/75, 3793/70, 3756/65, 3724/60, 3699/55,
3675/50, 3658/45, 3632/40, 3605/35, 3584/30,
3557/25, 3535/20, 3497/15, 3475/10, 3371/5,
3136/3
```

`SOC_Table_Default` 当前仍是 `SOC_TABLE_TEST` 自定义表的 RAM 初始值，不是当前默认产品的真实运行表。

电压、电流采样字段 `u16_VCellMax/u16_VCellMin/u16_Ichg/u16_Idsg` 上电时为全局零初始化值，主循环 200ms 周期进入 `App_SOC()` 后从 `g_stCellInfoReport` 刷新。

## 2. 后续烧录与快照兼容

普通 APP 烧录通常不会擦除后 64K 参数区，所以已经有效的 Flash 参数会优先于新固件默认值：

| 数据 | 存储位置 | 后续烧录默认是否覆盖 |
| --- | --- | --- |
| SOC 配置：容量、循环次数、0/100 电压、表选择 | RW 参数区 `0x0801C400/0x0801CC00` | 不自动覆盖 |
| SOC 运行快照：SOC、容量、循环、学习状态 | SOC journal `0x0801E000/0x0801E800` | 不自动覆盖 |
| SOC 默认 OCV 表 | 当前只在 RAM 中加载 | 新固件默认表会在每次启动加载；上位机写表当前不跨重启保存 |

SOC 快照已经升级为 V2。`StorageFlash_LoadSocData()` 会优先读取 V2；如果 V2 无效，再兼容读取旧 V1 快照，并用当前配置补齐 V2 字段，下一次保存会自动迁移为 V2。

| 字段 | V1 | V2 | 说明 |
| --- | --- | --- | --- |
| `u16SocNow` | 有 | 有 | 内部 SOC |
| `u16DsgSocInt` | 有 | 有 | 放电循环累计百分比 |
| `u32CycleTimes` | 有 | 有 | 循环次数，内部单位 `count * 100` |
| `u16FormatVersion` | 无 | 有 | V2 固定 `0x0002` |
| `u16MaxErrorPercent` | 无 | 有 | 粗略可信度/误差上限 |
| `u32CapNow` | 无 | 有 | 剩余容量，单位 `As * 10` |
| `u32CapFull` | 无 | 有 | 当前 `FCC/SOH` 容量基准 |
| `u32LearnPassedAs10` | 无 | 有 | 满/空锚点之间已通过电量 |
| `u16LearnAnchorSoc` | 无 | 有 | 当前学习锚点，0 或 100 |
| `u16LearnState` | 无 | 有 | 学习状态 |
| `u16Flags` | 无 | 有 | 学习标志，例如已学习 |
| `u16Reserved[4]` | 无 | 有 | 后续兼容预留 |

需要让后续固件主动改现场参数时，有四种方式：

1. 上位机写 `RS485_CMD_ADDR_SOC_AH` 起始的 SOC 配置参数，成功后走 `EEPROM_SaveRWParametersToFlash()` 落盘。
2. 修改 `UpgradeParamPolicy.h`：打开 `UPGRADE_PARAM_RESET_SOC_CONFIG`，并递增 `UPGRADE_PARAM_POLICY_VERSION`，让升级包一次性覆盖现场 SOC 配置。
3. 修改 `UpgradeParamPolicy.h`：打开 `UPGRADE_PARAM_RESET_SOC_SNAPSHOT`，并递增 `UPGRADE_PARAM_POLICY_VERSION`，让升级包一次性把历史 SOC 快照写回默认启动快照。
4. 擦除后 64K 参数区或用维护命令重置参数，让固件重新按编译期默认值初始化。

注意：`UPGRADE_PARAM_RESET_SOC_CONFIG` 只覆盖 SOC 配置，不会自动清空 SOC journal 中的历史 SOC 快照；`UPGRADE_PARAM_RESET_SOC_SNAPSHOT` 只重写 SOC journal。

## 3. 模块边界

本次优化保持模块边界简单：

| 模块 | 责任 |
| --- | --- |
| `SOC.c` | 加载配置、200ms 调度、把系统采样输入 SOC 模块 |
| `SocEnhance.c` | SOC 状态机、安时积分、端点校准、静置 OCV、弱单体保护、显示平滑、快照触发 |
| `Flash.c/.h` | SOC 快照 V1/V2 读写兼容和 journal 存储 |
| 文档 | 解释策略和测试边界 |

新增运行状态集中在 `SOC_RUNTIME_CONTEXT` 中，避免更多散落 `static` 状态。策略均为固定阈值和有限状态，不引入动态内存、浮点计算、后台任务或跨模块隐式依赖。

## 4. 运行数据流

`App_AFEGet()` 每 200ms 成功刷新一次 AFE 电流后递增 `g_u32AfeCurrentSampleSeq`。`App_SOC()` 同样挂在 200ms 时基上，但只消费新的 AFE 样本，避免通信忙导致 AFE 未刷新时重复积分旧电流。

当前 SOC 主流程：

1. `App_AFEGet()` 刷新电压、电流，并递增 AFE 样本序号。
2. `App_SOC()` 检查样本序号，有新样本才调用 `SOC_UpdateSampleData()`。
3. `SOC_IntEnhance_Ctrl()` 按“命令处理 → 电流模式判断 → 安时积分 → 满电确认 → 静置 OCV → 弱单体保护 → 快照持久化 → 输出同步”执行。
4. 上位机刷新命令仍保留：`1` 为 OCV 刷新，`2` 为容量/循环初始化，`3` 为设置 SOC 一次。
5. SOC、放电累计、循环次数、`FCC` 或学习状态变化时写入 SOC journal。
6. 对外 SOC 默认约 `1%/s` 向内部 SOC 靠近；首次启动、手动刷新和 RTC 休眠补偿会强制同步。

## 5. 核心策略

### 5.1 内部 SOC 与对外 SOC 分离

| 层级 | 变量 | 说明 |
| --- | --- | --- |
| 内部估算 SOC | `SOC_Calculate_Element.u8SOC_Now` | 算法真实状态，积分、端点、低压保护会直接修改 |
| 内部剩余容量 | `SOC_Calculate_Element.u32CapNow` | 单位 `As * 10`，基于当前 `FCC` 计算 |
| 对外发布 SOC | `SOC_Enhance_Element.u8_SOC` / `g_stCellInfoReport.SocElement.u16Soc` | 默认平滑跟随，供显示和通信使用 |

正常显示变化限制为约 `1%/s`。当弱单体进入 `V0 + 30mV` 临界窗口且对外 SOC 高于内部目标时，可按 200ms 样本快速下降，避免低压风险被高 SOC 掩盖。

### 5.2 `CHG / DSG / RELAX` 模式滞回

当前使用 `SOC_CURRENT_ENTER_A10 = 4`，单位 `A * 10`，即约 0.4A。该阈值高于现有 AFE 小电流清零区间，避免噪声触发积分。

| 条件 | 行为 |
| --- | --- |
| `u16_Ichg >= 0.4A` 且不小于放电电流 | 进入 `CHG`，本次样本按充电积分 |
| `u16_Idsg >= 0.4A` | 进入 `DSG`，本次样本按放电积分 |
| 两者都低于阈值 | 不积分；连续约 5s 无有效电流后进入 `RELAX` |

方向切换时会清空未满 1% 的容量余量、积分时间余量和端点计数，避免上一方向的小数余量带到反方向。

### 5.3 安时积分主路径

每个 200ms 样本的容量增量为：

```text
current(A * 10) * 200ms / 1000
```

内部容量单位仍为 `As * 10`。不能整除的余量保存在 `u32IntegrateRemainderMs`，小电流不会因为 200ms 周期被直接截断。

| 场景 | 内部处理 |
| --- | --- |
| 充电后 `u32CapNow >= cap_base` | `u32CapNow = cap_base`，`u8SOC_Now = 100` |
| 放电后 `u32CapNow <= 0` | `u32CapNow = 0`，`u8SOC_Now = 0`，触发可信空电锚点 |
| 未到边界但累计容量达到 1% | SOC 按累计百分比增减，余量保留到下一次积分 |

`cap_base` 优先使用学习后的 `u32CapFull`，无有效学习值时回退 `u32CapFactory`。

### 5.4 满电确认

充电末端分两层：

1. 端点电压逐步上修：靠近或超过 `V100` 时，仍按秒级窗口每次上修 1%，但普通充电路径最多上修到 99%，避免未经确认就显示满电。
2. 可信满电确认：只有满足全部条件并持续约 60s 后，才把内部 SOC 设为 100%，并触发可信满电锚点。

可信满电条件：

| 条件 | 目的 |
| --- | --- |
| 当前模式为 `CHG` | 必须处于真实充电过程 |
| `VCellMax >= V100` | 至少有单体达到满电端点 |
| `VCellMin >= SOC_GetFullCellConfirmVoltage()` | 防止只有最高单体虚高；当前三元锂默认 4000mV，`LIFEPO` 默认 3300mV |
| `Ichg != 0` 且 `Ichg <= C/20` | 必须进入 taper 阶段；27Ah 默认约 1.35A，下限不低于 0.4A |
| 持续约 60s | 防止瞬态误判 |

### 5.5 放电末端与弱单体保护

放电端点仍按 `V0` 附近窗口逐步下修：

| 条件 | 内部 SOC 修正 |
| --- | --- |
| `VCellMin <= V0 + 100mV` 且 `> V0`，当前 SOC `> 5` | 每 10s 降低 1% |
| `VCellMin <= V0`，当前 SOC `>= 5` | 每 4s 降低 1% |
| `VCellMin <= V0`，当前 SOC `< 5` | 每 8s 降低 1% |
| `VCellMin <= V0 - 50mV`，当前 SOC `> 0` | 每 2s 降低 1% |

弱单体保护在每次 `SOC_IntEnhance_Ctrl()` 中检查，并限制 SOC 上限：

| `VCellMin` 条件 | SOC 上限 | 最大单次下修 |
| --- | ---: | ---: |
| `<= V0` | 0% | 5% |
| `<= V0 + 30mV` | 2% | 2% |
| `<= V0 + 60mV` | 4% | 1% |
| `<= V0 + 90mV` | 6% | 1% |
| `<= V0 + 120mV` | 8% | 1% |

弱单体达到 `V0` 且内部 SOC 被压到 0 时，触发可信空电锚点。

### 5.6 静置 OCV 与 RTC 补偿

运行态静置必须满足：`RELAX` 模式、无有效充/放电、单体电压有效，并且电压在 30s 内稳定在约 3mV 窗口内，才允许 OCV 修正。

静置时间分桶：

| 静置时间 | bucket | 最大修正步长 |
| ---: | ---: | ---: |
| `< 10min` | 0 | 不修正 |
| `10min ~ 30min` | 1 | 向下最多 1% |
| `30min ~ 1h` | 2 | 向下最多 1% |
| `1h ~ 6h` | 3 | 向下最多 2%；若当前 SOC `>= 90`，可向上最多 1% |
| `>= 6h` | 4 | 向下最多 3%；若当前 SOC `>= 90`，可向上最多 2% |

`LFP/LiFePO2` 平台区中段只做保守修正，避免 OCV 表平台误差造成明显跳变。`SOC_ApplyRtcRelaxationCompensation()` 用 RTC 休眠秒数走同一套 OCV 修正，但由于本身来自长时间休眠唤醒，会强制同步对外 SOC。

### 5.7 轻量 `FCC/SOH` 学习

学习只在可信满电/空电锚点之间统计 passed charge：

| 项 | 当前策略 |
| --- | --- |
| 学习触发 | 满电锚点到空电锚点，或空电锚点到满电锚点 |
| 首次学习跨度 | 至少 90% SOC |
| 后续学习跨度 | 至少 40% SOC |
| 容量允许范围 | 出厂容量的 50% ~ 110% |
| 单次变化限制 | 不超过旧 `FCC` 的 5% |
| 学习成功后 | 设置 `SOC_LEARN_FLAG_LEARNED`，`u16MaxErrorPercent` 收敛到 5 |

方向反转且已经累计 passed charge 时，会放弃本轮学习，防止半程充放电把 `FCC` 学坏。

## 6. 手动校准入口

| 入口 | 标志/函数 | 行为 |
| --- | --- | --- |
| OCV 刷新 | `u16_RefreshData_Flag = 1` | 按当前 `VCellMin` 查 OCV 表；非充电状态下只允许降低 SOC，不允许抬高 SOC |
| 容量/循环初始化 | `u16_RefreshData_Flag = 2` | 清空放电循环累计，重新加载容量配置，`u32CapFull = u32CapFactory`，清除学习标志 |
| 设置一次 SOC | `u16_RefreshData_Flag = 3` + `u8_SetSocOnce` | 把内部 SOC 设置为指定 `0~100`，按容量基准重算 `u32CapNow`，强制同步对外 SOC |
| SOC 固定 | `b1OnOFF_SOC_Fixed` | 对外发布 SOC 强制为 60%，不改变内部积分状态 |
| SOC 清零显示 | `b1OnOFF_SOC_Zero` | 对外发布 SOC 强制为 0%，不等价于清空内部容量快照 |

## 7. 可调整参数

### 7.1 上位机/RW 参数可调整项

| 参数 | 当前默认值 | 作用 | 生效方式与注意点 |
| --- | ---: | --- | --- |
| `OtherElement.u16Soc_Ah` | `270` | 出厂容量，单位 `10 * Ah` | 通过 `RS485_CMD_ADDR_SOC_AH` 写入后保存；影响积分分母和容量显示 |
| `OtherElement.u16Soc_Cycle_times` | `3` | 初始循环次数 | 历史 SOC 快照存在时，运行循环次数优先来自 SOC journal |
| `OtherElement.u16Soc_V_100` | `4180mV` | 满电端点电压 `V100` | 当前枚举名为 `RS485_CMD_ADDR_SOC_RES1` |
| `OtherElement.u16Soc_V_0` | `3000mV` | 空电端点电压 `V0` | 当前枚举名为 `RS485_CMD_ADDR_SOC_RES2` |
| `OtherElement.u16Soc_TableSelect` | `SOC_TABLE_TERNARYLI` | OCV 表选择 | 当前位于 `OtherElement` offset 12 |
| `SOC_Table_Set[42]` | `SOC_Table_Default` | 自定义 OCV 表，21 组 `电压/SOC` | 只有选择 `SOC_TABLE_TEST` 时用于查表；当前只在 RAM 中生效 |
| `u8_SetSocOnce` | 无固定默认 | 一次性设置 SOC | 写 `0~100` 后触发 `u16_RefreshData_Flag = 3` |

### 7.2 源码常量可调整项

| 参数 | 当前值 | 作用 | 修改风险 |
| --- | ---: | --- | --- |
| `SOC_DEFAULT_STARTUP_PERCENT` | `60%` | 无有效 SOC 快照时的默认启动 SOC | 首次烧录或 SOC journal 无效时生效 |
| `SOC_CURRENT_ENTER_A10` | `4` | 充/放电模式进入阈值，单位 `A * 10` | 改小可能受采样零点噪声影响，改大会漏掉小电流 |
| `SOC_MODE_RELAX_ENTRY_SECONDS` | `5s` | 无有效电流后进入 `RELAX` 的滞回时间 | 改小会增加模式抖动 |
| `SOC_FULL_CONFIRM_SECONDS` | `60s` | 满电确认保持时间 | 改小会增加满电误判风险 |
| `SOC_DISPLAY_STEP_SECONDS` | `1s` | 对外 SOC 正常平滑步进周期 | 改大显示更稳但跟随更慢 |
| `SOC_RELAX_STABLE_SECONDS` | `30s` | 运行态静置 OCV 的电压稳定时间 | 改小会放大瞬态电压影响 |
| `SOC_RELAX_VOLT_STABLE_WINDOW_MV` | `3mV` | 静置电压稳定窗口 | 需结合 AFE 精度和噪声实测 |
| `SOC_REST_BUCKET_1_SECONDS` | `600` | 静置 OCV 第一档 | 改小会增加静置修正频率 |
| `SOC_REST_BUCKET_2_SECONDS` | `1800` | 静置 OCV 第二档 | 同上 |
| `SOC_REST_BUCKET_3_SECONDS` | `3600` | 静置 OCV 第三档 | 同上 |
| `SOC_REST_BUCKET_4_SECONDS` | `21600` | 静置 OCV 第四档 | 同上 |
| `SOC_WEAK_CELL_GUARD_WINDOW_MV` | `120mV` | 弱单体保护总窗口 | 改大后 SOC 会更早被低压单体压低 |
| `SOC_WEAK_CELL_CRITICAL_WINDOW_MV` | `30mV` | 弱单体临界窗口 | 影响快速下修和显示快速下降 |
| `SOC_LEARN_FIRST_SPAN_PERCENT` | `90%` | 首次 FCC 学习所需跨度 | 改小会增加误学习风险 |
| `SOC_LEARN_NEXT_SPAN_PERCENT` | `40%` | 后续 FCC 学习所需跨度 | 改小会更灵敏但更容易受片段循环影响 |
| `SOC_LEARN_CAP_MIN/MAX_PERCENT` | `50%/110%` | 学习容量允许范围 | 超出范围会拒绝学习 |
| `SOC_LEARN_CAP_MAX_STEP_PERCENT` | `5%` | 单次 FCC 变化限制 | 改大可能造成 SOH/SOC 跳变 |
| `SOC_CYCLE_PERCENT_PER_COUNT` | `80%` | 放电累计多少百分比计 1 次循环 | 改动会影响循环次数增长速度 |

## 8. 当前边界

1. `SOC_Table_Set` 仍是 RAM 表，上位机写自定义 OCV 表后不会跨重启保存。
2. `u32LearnPassedAs10` 不作为单独 Flash 写入触发条件，避免每个小积分都写 Flash；当 SOC、循环、`FCC` 或学习状态变化保存时会随 V2 快照一起落盘。
3. 现有策略仍是保护板场景的中等增强方案，不等价于专用 fuel gauge 的 `Impedance Track` 或 `ModelGauge`。
