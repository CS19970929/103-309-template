# SOC Keil Watch 在线调试说明（2026-05-12）

## 开启方式

量产默认必须保持：

```c
#define PROJECT_CFG_DEBUG_WATCH_ENABLE 0
```

需要 Keil 在线调试时，直接选择 Keil 目标 `FD_Debug`。当前工程已在 `FD_Debug` 中定义：

```c
#define PROJECT_CFG_BUILD_PROFILE 1
#define PROJECT_CFG_DEBUG_WATCH_ENABLE 1
```

手工新建调试目标时，也必须使用同样的两个定义。
`Project_BuildGuard.h` 会阻止 Release 构建开启 `PROJECT_CFG_DEBUG_WATCH_ENABLE`。

## Watch 入口

打开后，在 Keil Watch 添加：

```c
g_dbg_soc_watch
g_dbg_soc_watch->u8Mode
g_dbg_soc_watch->u8InternalSoc
g_dbg_soc_watch->u8DisplaySoc
g_dbg_soc_watch->u8DeferredOcvTarget
g_dbg_soc_watch->u8LastCalibSource
g_dbg_soc_watch->u8LastBlockReason
```

该入口是指向 `SocEnhance.c` 内部 SOC 状态的调试镜像，不改变业务逻辑，不把 `s_soc` 直接暴露为全局变量。

## 关键字段

| 字段 | 含义 |
| --- | --- |
| `u8Mode` | 当前 SOC 模式：`0=RELAX`，`1=CHG`，`2=DSG` |
| `u8InternalSoc` | 内部真实 SOC |
| `u8DisplaySoc` | 对外显示/通信发布 SOC |
| `u8Soh` | SOH |
| `u32CapNowAs10` | 当前容量，单位 `As*10` |
| `u32CapFullAs10` | 满充有效容量，单位 `As*10` |
| `u32CycleX100` | 循环次数，单位 `cycle*100` |
| `u16VCellMin/u16VCellMax` | 当前 SOC 使用的最低/最高单体电压 |
| `u16Ichg/u16Idsg` | 当前 SOC 使用的充/放电电流，单位 `A*10` |
| `u16FullTicks` | 满电确认计数 |
| `u16EmptyTicks` | 低压尾段计数 |
| `u16MidTicks` | 中低压弱约束计数 |
| `u16SagHoldTicks` | 大电流压降/回弹保护剩余 tick |
| `u32RestTicks` | 静置累计 tick |
| `u32StableRestTicks` | 静置电压稳定累计 tick |
| `u32ShortRestTicks` | OCV target 刷新/消化节拍 tick |
| `u32LongRestDownTicks` | 久置低 OCV 静置下修 tick |
| `u8DeferredOcvValid` | 是否存在待消化 OCV target |
| `u8DeferredOcvTarget` | 待消化 OCV target |
| `u8LowTailActive` | 当前 tick 是否命中低压尾段表 |
| `u16EmptyTailTarget/u16EmptyTailTicks` | 当前低压尾段目标和周期 |
| `u8MidTailActive` | 当前 tick 是否命中中低压弱约束 |
| `u16MidTailTarget/u16MidTailTicks` | 当前中低压弱约束目标和周期 |
| `u8CalibrationAllowed` | 当前电压/故障门控是否允许校准 |
| `u8SagHoldBlocksCalibration` | sag holdoff 是否正在阻断电压校准 |
| `u8RestVoltageStable` | 当前静置电压是否被认为稳定 |
| `u8LastSocBefore/u8LastSocAfter` | 最近一次 SOC 变化前后值 |

## `u8LastCalibSource` 对照

| 值 | 来源 |
| ---: | --- |
| `0` | 无 |
| `1` | 充电积分 |
| `2` | 放电积分 |
| `3` | 满电锚点 |
| `4` | 低压尾段 |
| `5` | 中低压弱约束 |
| `6` | 静置记录 OCV target |
| `7` | 后续充/放电消化 deferred OCV |
| `8` | 久置低 OCV 静置下修 |
| `9` | 手动 OCV 刷新 |
| `10` | SOC 参数/容量刷新 |
| `11` | 设置一次 SOC |
| `12` | 启动从 Flash 快照恢复 |
| `13` | 启动按 OCV 表初始化 |
| `14` | 启动默认 60% |
| `15` | RTC 静置补偿 |

## `u8LastBlockReason` 对照

| 值 | 原因 |
| ---: | --- |
| `0` | 无阻断 |
| `1` | 电压无效 |
| `2` | 单体压差超限 |
| `3` | 保护故障阻断 |
| `4` | 系统故障阻断 |
| `5` | 大电流 sag/rebound holdoff 阻断 |
| `6` | OCV target 与当前充放电方向不匹配 |
| `7` | 当前不是 RELAX，不能累计静置 |
| `8` | 低压尾段已生效，静置 OCV 被让位 |
| `9` | 静置电压不稳定 |
| `10` | SOC_Fixed 阻止手动 OCV 刷新 |
| `11` | SOC_Zero 阻止容量参数刷新 |

## 推荐观察组合

### 低电末端掉得快

```c
g_dbg_soc_watch->u8InternalSoc
g_dbg_soc_watch->u8DisplaySoc
g_dbg_soc_watch->u16VCellMin
g_dbg_soc_watch->u16Idsg
g_dbg_soc_watch->u8LowTailActive
g_dbg_soc_watch->u16EmptyTailTarget
g_dbg_soc_watch->u16EmptyTailTicks
g_dbg_soc_watch->u8LastCalibSource
```

如果 `u8LastCalibSource=4`，说明是低压尾段表在下修；再结合 `u16EmptyTailTarget/Ticks` 判断应该调 `PROJECT_CFG_SOC_EMPTY_TAIL_*` 哪个参数。

### 静置校准慢或不生效

```c
g_dbg_soc_watch->u8Mode
g_dbg_soc_watch->u32RestTicks
g_dbg_soc_watch->u32StableRestTicks
g_dbg_soc_watch->u32ShortRestTicks
g_dbg_soc_watch->u8RestVoltageStable
g_dbg_soc_watch->u8DeferredOcvValid
g_dbg_soc_watch->u8DeferredOcvTarget
g_dbg_soc_watch->u8LastBlockReason
```

如果 `u8RestVoltageStable=0` 或 `u8LastBlockReason=9`，说明电压回弹/波动还没稳定，不建议直接缩短静置时间。

### 爬坡后 SOC 突然变化

```c
g_dbg_soc_watch->u16SagHoldTicks
g_dbg_soc_watch->u8SagHoldBlocksCalibration
g_dbg_soc_watch->u8LastBlockReason
g_dbg_soc_watch->u8LowTailActive
```

正常情况下，大电流后 `u16SagHoldTicks` 会倒计时，且 `u8SagHoldBlocksCalibration=1` 时应阻止 OCV/中低压误校准。

## 验证入口

`tools/run_soc_host_c_test.py` 会编译两份宿主测试：

- 默认量产配置。
- `PROJECT_CFG_BUILD_PROFILE=1` + `PROJECT_CFG_DEBUG_WATCH_ENABLE=1` 的 Debug Watch 配置。

因此每次修改 SOC Watch 字段后，至少运行：

```powershell
py tools\run_soc_host_c_test.py
```
