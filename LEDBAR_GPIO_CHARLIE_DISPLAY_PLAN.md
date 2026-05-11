# LedBar GPIO Charlieplexing 显示方案

> 2026-05-11 更新：当前实现已经按 GPIO Charlieplexing 单路径重写，旧 `74HC595` 兼容代码已删除。后续维护以 [数码管 GPIO Charlie 重写说明](数码管GPIO查理复用重写说明.md) 为准。

## 需求

数码管控制方案从旧兼容路径收敛为 MCU GPIO 直接控制，不再保留 `74HC595` 方案代码。

新方案使用 5 根 MCU GPIO 直接连接 5pin 数码管，按查理复用方式扫描：每次只驱动一个目标段，一根 GPIO 输出高电平，一根 GPIO 输出低电平，其余 GPIO 保持高阻。这样软件可以避免 74HC595 推挽直推时无法关闭非目标线导致的串亮问题。

## 新 GPIO 定义

默认 pin 顺序定义在 `103 + 309/Project/Source/LedBar.h`：

| 查理复用线 | 原功能名 | MCU GPIO |
| --- | --- | --- |
| P1 | DBLED | PB15 |
| P2 | SPI1_NSS | PA4 |
| P3 | SPI1_SCK | PA5 |
| P4 | SPI_MOSI | PA6 |
| P5 | SEG_EN | PB10 |

相关引脚宏为：

```c
#define LEDBAR_GPIO_P1 GPIO_DBG_LED
#define LEDBAR_PIN_P1  PIN_DBG_LED
#define LEDBAR_GPIO_P2 GPIO_SPI1_NSS
#define LEDBAR_PIN_P2  PIN_SPI1_NSS
#define LEDBAR_GPIO_P3 GPIO_SPI1_SCK
#define LEDBAR_PIN_P3  PIN_SPI1_SCK
#define LEDBAR_GPIO_P4 GPIO_SPI_MOSI
#define LEDBAR_PIN_P4  PIN_SPI_MOSI
#define LEDBAR_GPIO_P5 GPIO_SEG_EN
#define LEDBAR_PIN_P5  PIN_SEG_EN
```

`SEG_EN` 在新方案中不再是全局数码管使能脚，而是第 5 根查理复用线。

## 软件实现

- `LedBar.c` 不再保留旧图样表、贪心图样生成和驱动选择宏。
- GPIO 方案把当前显示内容展开为段 ID 列表。
- TIM4 仍作为扫描定时器，1ms 中断内调用 `LedBar_Scan1ms()`，每次扫描一个目标段。
- 输出段前先把 5 根线全部设为输入浮空，再配置目标低端为推挽低电平、目标高端为推挽高电平。
- 显示关闭、休眠和 STOP 前会关闭 TIM4，并把 5 根显示线切到高阻或模拟输入。

## 冲突处理

- `SOC.c` 中原来的 `MCUO_DEBUG_LED1` 周期翻转已在新方案下关闭，避免 PB15 干扰显示。
- `conf.c` 中原来对 `SPI_NSS/SCK/MOSI`、`DBLED`、`SEG_EN` 的固定输出初始化已在新方案下改为输入浮空。
- 低功耗 IO 配置中不再把 `SEG_EN/PB10` 作为独立使能脚输出低电平。

## 验证建议

1. 上电后未请求显示时，PB15、PA4、PA5、PA6、PB10 应保持高阻或低功耗输入状态。
2. 按键请求 SOC 显示时，用示波器检查 5 根线：任一时刻应只有一根高、一根低，其余高阻。
3. 使用 `LedBar_EnableSingleSegmentTest()` 和 `LedBar_SetSingleSegmentIndex(0~17)` 逐段确认丝印和段位映射。
4. 验证 SOC `0~100`、充电图标和百分号显示；新 GPIO 方案理论上可以逐段精确显示，不再受 74HC595 推挽图样限制。
