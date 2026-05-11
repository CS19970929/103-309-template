# 数码管 GPIO Charlie 重写说明

## 目标

本次重写把数码管代码收敛为单一 GPIO Charlieplexing 路径，删除旧 `74HC595` 兼容分支、最优图样表、贴近显示策略和驱动选择宏。后续数码管功能应优先在当前框架上扩展，不再恢复 `74HC595` 相关代码。

## 当前需求

| 场景 | 行为 |
| --- | --- |
| 上电、复位、休眠唤醒后重新运行 | 主动显示 SOC 约 10 秒 |
| DI1 短按 | 显示当前 SOC，松手后约 5 秒熄灭 |
| DI1 长按约 3 秒 | 保存休眠前 SOC，并进入深度休眠流程 |
| 休眠中短按 | 显示 BKP 中保存的休眠前 SOC，超时后回到 STOP |
| `GPIO_MCU_WK` 高电平 | 持续显示 SOC，并叠加充电图标 |
| 充电电流有效 | 显示 `SOC + % + 充电图标` |
| 无显示请求 | 熄屏、停止 TIM4、显示 GPIO 输出低电平 |

显示 SOC 只读取 `g_stCellInfoReport.SocElement.u16Soc`，保证数码管、CAN、RS485 对外 SOC 口径一致。

## 代码边界

| 层级 | 职责 | 主要函数 |
| --- | --- | --- |
| GPIO/TIM4 驱动层 | 管脚高阻、目标段高低电平输出、TIM4 扫描开关 | `LedBar_GpioInitForDisplay()`、`LedBar_OutputRoute()`、`LedBar_StartScanTimer()` |
| 显示构建层 | `SOC + icons` 转成 route frame | `LedBar_BuildTargetMask()`、`LedBar_BuildFrameFromMask()` |
| 业务服务层 | 按键窗口、启动窗口、`MCU_WK` 滤波、充电图标滤波 | `LedBar_ServiceSwitch()`、`LedBar_ServiceMcuWakeFilter()`、`LedBar_ServiceChargeFilter()` |
| 对外入口层 | 保持旧 API，供主循环、休眠、测试入口调用 | `APP_LedBar()`、`LedBar_Clear()`、`LedBar_PrepareForStop()` |

`LedBar.c` 不再包含显示驱动双路径条件编译。量产路径只有一套：每次扫描一个 route，一根 GPIO 输出高电平，一根 GPIO 输出低电平，其余 GPIO 保持高阻。

## 保留接口

| 接口 | 保留原因 |
| --- | --- |
| `LedBar_Init()` | 初始化显示模块和 GPIO 安全态 |
| `LedBar_Scan1ms()` | TIM4 中断扫描入口 |
| `LedBar_SetNumber()` / `LedBar_SetIndicators()` | 保留手动显示/后续调试扩展入口 |
| `LedBar_EnableSingleSegmentTest()` / `LedBar_SetSingleSegmentIndex()` | 上板逐段确认 `0~17` 段位 |
| `LedBar_SaveSleepSoc()` / `LedBar_LoadSleepSoc()` | 休眠前 SOC 预览 |
| `LedBar_PrepareForStop()` | STOP 前关闭扫描和显示 GPIO |
| `APP_LedBar()` | 主循环业务入口 |

## 后续扩展规则

1. 新增显示内容时，先增加 route 或 icon 定义，再在 `LedBar_BuildTargetMask()` 里组合目标段。
2. 新增显示触发条件时，只改 `LedBar_IsDisplayRequested()` 或 `APP_LedBar()` 的业务仲裁，不要改 TIM4 扫描逻辑。
3. 新增闪烁、故障优先级、动画时，应作为独立显示模式处理，不要把特殊逻辑塞进 GPIO 输出函数。
4. STOP 前必须经过 `LedBar_PrepareForStop()` 或 `LedBar_Clear()`，保证 TIM4 关闭、GPIO 进入低功耗安全态。
5. 不再引入 `74HC595`、图样贴近、串亮评分等历史策略。

## 验收点

| 类别 | 验收项 |
| --- | --- |
| 单段测试 | `0~17` 段位逐段点亮，与实物丝印一致 |
| SOC 显示 | 重点检查 `0, 1, 8, 10, 11, 31, 41, 47, 99, 100` |
| 图标 | `%` 常亮；充电或 `MCU_WK` 有效时充电图标亮 |
| 按键 | DI1 短按 5 秒显示；长按约 3 秒进入深度休眠 |
| 启动 | 上电、复位、休眠唤醒后显示约 10 秒 |
| 低功耗 | 无显示请求时 TIM4 关闭，显示 GPIO 输出低电平 |
