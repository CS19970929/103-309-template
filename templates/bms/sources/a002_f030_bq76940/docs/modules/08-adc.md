# ADC 与模拟量采样

## 相关文件

- [ADC.c](../../Code/Source/ADC.c)
- [ADC.h](../../Code/Include/ADC.h)

## 模块职责

ADC 模块负责采集外部温度和 MOS 温度等模拟量，并将 ADC 原始值换算为业务可用温度数据。

## 硬件资源

| 采样项 | ADC 通道 | GPIO | 说明 |
| --- | --- | --- | --- |
| EV1 温度 | `ADC_Channel_4` | PA4 | 外部温度。 |
| EV2 温度 | `ADC_Channel_5` | PA5 | 外部温度，与 LED Bar 条件功能冲突。 |
| MOS 温度 | `ADC_Channel_8` | PB0 | MOS 温度保护。 |

## ADC 配置

| 项目 | 配置 |
| --- | --- |
| ADC | ADC1 |
| DMA | DMA1 Channel1 |
| DMA 模式 | Circular |
| 触发源 | TIM15 TRGO |
| 分辨率 | 12bit |
| 扫描方向 | Upward |
| 采样时间 | 55.5 cycles |
| 数据缓冲 | `g_u16ADCValFilter[ADC_NUM]` |

TIM15 配置为周期触发 ADC 扫描，避免主循环软件轮询启动 ADC。

## 主调度

`App_AnlogCal()` 在主循环中执行，启动初期会等待 ADC 稳定计数。当前实际处理重点是 `ADC_TTC()` 温度查表与滤波。

## 与保护模块关系

ADC 温度结果会进入 `g_stCellInfoReport`，再被 `Fault` 用于：

- 充电高温/低温保护。
- 放电高温/低温保护。
- MOS 高温保护。
- 温度断线或异常监控。

## 维护建议

- 新增 ADC 通道时必须同步更新 `ADC_NUM`、DMA 缓冲区解析顺序、GPIO analog 配置和上层温度/电压换算逻辑。
- PA5、PA6 等资源存在条件功能冲突，启用 LED/PWM 前必须确认 ADC 是否仍需要。
- 温度查表应复用公共插值逻辑，避免多个模块各自维护 NTC 曲线。
