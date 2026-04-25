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

需要让后续固件主动改现场参数时，有三种方式：

1. 上位机写 `RS485_CMD_ADDR_SOC_AH` 起始的 SOC 配置参数，成功后会走 `EEPROM_SaveRWParametersToFlash()` 落盘。
2. 修改 `UpgradeParamPolicy.h`：打开 `UPGRADE_PARAM_RESET_SOC_CONFIG`，并递增 `UPGRADE_PARAM_POLICY_VERSION`，让升级包一次性覆盖现场 SOC 配置。
3. 擦除后 64K 参数区或用维护命令重置参数，让固件重新按编译期默认值初始化。

注意：`UPGRADE_PARAM_RESET_SOC_CONFIG` 只覆盖 SOC 配置，不会自动清空 SOC journal 中的历史 SOC 快照。要改当前 SOC，可使用设置 SOC 一次的通信命令；要让板子完全回到首次快照，需要擦除或重置 SOC journal。

## 3. 运行逻辑

`App_SOC()` 每 200ms 执行一次：

1. 刷新电压、电流输入。
2. 发布上一轮 SOC 输出。
3. `SOC_IntEnhance_Ctrl()` 按状态机处理充电积分、放电积分、静置 OCV 补偿、弱单体保护修正。
4. 处理上位机刷新命令：`1` 为 OCV 刷新，`2` 为容量/循环初始化，`3` 为设置 SOC 一次。
5. SOC、放电累计或循环次数变化时写入 SOC journal。
6. 每 1s 平滑一次对外显示 SOC。

本次整理后，SOC 初始化集中为三步：加载容量配置、清零计算状态、读取或创建 SOC 快照；删除了没有调用路径或没有计算作用的旧字段和旧初始化函数。
