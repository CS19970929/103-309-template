# SOC/SOH 估算

## 相关文件

- [SOC.c](../../Code/Source/SOC.c)
- [SOC.h](../../Code/Include/SOC.h)
- [SocEnhance.c](../../Code/Source/SocEnhance.c)
- [SocEnhance.h](../../Code/Source/SocEnhance.h)
- [DataDeal.c](../../Code/Source/DataDeal.c)

## 模块职责

SOC 模块负责估算电池剩余容量、SOC、SOH、满充容量、剩余容量和循环次数。算法综合使用 OCV 表、电流积分、运行状态、容量参数和内部 Flash 持久化数据。

## 默认电芯体系

当前 `main.h` 默认启用 `LIFEPO`，即磷酸铁锂 OCV/SOC 参数路径。代码中也保留三元体系相关表或条件路径。

## 主调度

`App_SOC()` 在主循环中执行，通常受 `gu8_200msAccClock_Flag` 调度。启动阶段会从内部 Flash 初始化 SOC 参数和 OCV 表。

## 输入数据

| 输入 | 来源 |
| --- | --- |
| 当前电流 | `DataDeal` |
| 单体电压/总压 | `DataDeal` |
| 充放电状态 | `IO_Control` / 电流方向 |
| OCV 表 | 内部 Flash 默认/保存数据 |
| 电池容量参数 | 内部 Flash `OtherElement` |
| RTC 或休眠计数 | `RTC` / 内部 Flash |

## 输出数据

| 输出 | 去向 |
| --- | --- |
| SOC | `g_stCellInfoReport.SocElement`、通信、LED。 |
| SOH | 通信、记录。 |
| 剩余容量 | 通信、保护。 |
| 满充容量 | 通信、寿命估算。 |
| 循环次数 | 内部 Flash 与通信。 |

## 算法组成

- OCV 初始校准：根据静置电压估算初始 SOC。
- Coulomb counting：根据电流积分修正剩余容量。
- 充满/放空状态修正：在明确满充或欠压场景下校准端点。
- 内部 Flash 持久化：周期性保存关键 SOC 参数，避免掉电丢失。
- 增强 SOC：`SocEnhance` 提供额外参数和修正逻辑。

## RTC 相关路径

`SOC_OCV_Fix()` 等部分休眠后 OCV 修正路径受 `__FUNC_RTC__` 控制。当前 `__FUNC_RTC__` 默认未启用，因此应按实际宏状态判断算法覆盖范围。

## 维护建议

- 修改容量单位或电流单位前必须同步 SOC、电流保护、通信协议和校准参数。
- OCV 表长度和内部 Flash 逻辑地址固定，替换电芯体系时要同步默认表、写入长度和上位机工具。
- SOC 算法参数变更应保留旧内部 Flash 数据兼容或增加参数版本。
