# SOC 自耗与校准优化说明

日期：2026-05-19

范围：主工程 `103 + 309`，文件 `Project/Source/SocEnhance.c`。

## 目标

- 去除 `SOC_State_Transfer`、`SOC_Data_Filter` 等会拖慢 SOC 响应的状态/数据滤波路径。
- 将 SOC 计算改为 200ms 周期的库仑积分，按真实电流和时间换算容量变化。
- 增加端点校准、静置 OCV 校准、开机 OCV 回退等策略，保证显示体验和边界可靠性。
- 增加 BMS 自耗配置，默认 15mA，通过代码宏配置，不需要用户协议配置。
- 自耗只影响空闲时的剩余容量和显示 SOC，不参与主充放电库仑积分、循环次数、满空校准累计。

## 配置宏

`SocEnhance.c` 中新增自耗宏：

```c
#ifndef SOC_SELF_CONSUMPTION_ENABLE
#define SOC_SELF_CONSUMPTION_ENABLE 1
#endif

#ifndef SOC_SELF_CONSUMPTION_CURRENT_MA
#define SOC_SELF_CONSUMPTION_CURRENT_MA ((UINT16)15)
#endif
```

如需调整板端静态功耗，只改 `SOC_SELF_CONSUMPTION_CURRENT_MA`。如果需要完全关闭自耗补偿，将 `SOC_SELF_CONSUMPTION_ENABLE` 置 0。

保留 `SOC_SELF_CONSUME_CURRENT_MA` 兼容旧命名，但推荐使用 `SOC_SELF_CONSUMPTION_CURRENT_MA`。

## 计算策略

1. 充电、放电、静置由当前采样值直接判断，不再经过 `SOC_CALI_STATE_TRANSFER` 延时状态机。
2. 充放电容量变化使用 `current_ma * 200ms` 累加，保留毫安毫秒余数，避免小电流长期丢量。
3. SOC 百分比变化仍按标称容量换算，余数继续保留，避免整百分比截断导致的跳变。
4. 方向切换时清零主积分余量，避免充放电快速切换后互相污染。
5. 放电循环次数只由真实放电积分触发，自耗不累计循环次数。

## 校准策略

- 开机 EEPROM 中 SOC 非法时，优先用有效单体电压做 OCV 估算，电压无效时回退 60%。
- 满电端：充电且单体最高电压到达满电阈值、最低单体满足材料体系下限后，延时确认到 100%。
- 空电端：放电且最低单体到达空电阈值后，延时确认到 0%。
- 末端渐进校准：接近满电或空电阈值时按多级计时逐步拉近 SOC；默认 L1/L2 每 60s 修正 1%，L3/强制段每 30s 修正 1%，避免几秒级快速跳变。
- 满空锚点：满电和空电直接锚定默认都需要持续 30s，避免电压瞬态触发。
- 静置 OCV：无充放电电流持续 1800s 后启用；只允许向下修正，超过 5% 死区才每 600s 修正 1%，永远不允许静置向上校准。
- 电压合法性：单体电压需在 2000mV 到 5000mV 且最高/最低压差小于 600mV，异常采样不触发端点和 OCV 校准。

## 自耗隔离

自耗使用独立的 `s_u32SelfConsumeMaMs` 与 `s_u32SelfConsumeCapChange`：

- 不写 `SOC_Calculate_Element.u32CapChange`。
- 不写 `u8DSG_SOC_Int` 和 `u32Cycle_times`。
- 只在无充电、无放电的静置路径执行。
- 方向切换、外部重设 SOC、满电/空电锚定时清零自耗余量。
- 放电空点和静置向下 OCV 校准优先级高于自耗，避免自耗干扰校准判断；静置满电电压不再把 SOC 上拉到 100%。

## 待验证

- EEPROM 为空值或 SOC 非法时，开机 SOC 能按 OCV 或 60% 回退。
- 低压放电持续到空电阈值后 SOC 归 0，充电或静置低压不会误归零。
- 满电充电条件满足后 SOC 能稳定到 100%。
- 静置 1800s 后 OCV 校准只做向下慢速修正，不出现大跳变，且不会把 SOC 往上校准。
- 15mA 自耗在静置时随时间缓慢降低 SOC，且不增加循环次数、不影响充放电积分余量。
