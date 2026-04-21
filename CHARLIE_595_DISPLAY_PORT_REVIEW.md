# Charlie + 74HC595 显示框架审查与移植说明

## 1. 任务背景

本次工作目标：

- 阅读 `charlie_595_display_framework` 文档与源码
- 审查框架本身的问题和可优化点
- 将其移植到当前 `103 + 309` 工程
- 使用 `conf_gpio.h` 中已定义的 SPI IO 驱动 `74HC595`
- 使用 `charlie_595_display_framework/LedBar.c` 中已经验证过的 5 线 Charlie 路由

当前工程最终落地文件：

- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/LedBar.h`
- `103 + 309/Project/Source/conf/conf_gpio.h`
- `103 + 309/Project/Source/main.c`

---

## 2. 框架 Code Review 结论

### 2.1 `display_debug_stub.c` 调试接口没有闭环

文件：

- `103 + 309/Project/Source/charlie_595_display_framework/src/display_debug_stub.c`

问题：

- 该文件依赖 `display_debug_set_all_hiz()`、`display_debug_set_pin_mode()`、`display_debug_set_pin_write()` 三个外部符号
- 框架里没有任何正式实现
- 如果把这个文件直接加入工程，会在链接阶段失败

结论：

- 这部分不能直接拿来用，必须并入正式驱动或补完整调试接口

---

### 2.2 `display_types.h` 的 `display_sel_t` 容易误导

文件：

- `103 + 309/Project/Source/charlie_595_display_framework/inc/display_types.h`

问题：

- `display_sel_t` 的枚举值写成了 `1~5`
- 但 `display_charlie.c` 实际使用的是 `sel_595_bit` 位索引，语义是 `0~7`
- 这两个概念容易被后续维护者混用

结论：

- 该枚举目前未被实际使用，但保留在框架里会制造“可直接拿来当 bit index 用”的错觉

---

### 2.3 扫描路径里重复清零 74HC595，亮度会被白白吃掉

文件：

- `103 + 309/Project/Source/charlie_595_display_framework/src/display_charlie.c`

问题：

- `display_light_one_led()` 每次点亮前都先 `display_disable_all_output()`
- `display_disable_all_output()` 又会调用 `disp595_clear_all()`
- 这意味着每个 LED 扫描周期都多做一次完整的 8bit 移位和锁存

影响：

- 占空比下降
- 亮度下降
- 总线时序冗余

结论：

- 正式移植时应只把 Charlie 线先切高阻，再切换 595 位选，避免每次都先写一遍 `0x00`

---

### 2.4 文档模型和测试代码的实际硬件模型不完全一致

文件：

- `103 + 309/Project/Source/charlie_595_display_framework/README.md`
- `103 + 309/Project/Source/charlie_595_display_framework/LedBar.c`

问题：

- README/文档把硬件描述成 “百位 1 + 十位 + 个位 + 充电 + %”
- 但测试版 `LedBar.c` 的实际路由命名是 `1A~1G`、`2A~2G`、`H1~H4`
- 也就是说，文档是抽象模型，测试代码才是实际实测路由

结论：

- 移植时不能只看文档里的占位映射表，必须以测试版 `LedBar.c` 的路由为准

---

### 2.5 `display_hw_map.c` 只是示例映射，不能直接量产

文件：

- `103 + 309/Project/Source/charlie_595_display_framework/src/display_hw_map.c`

问题：

- 文件头注释已经说明“下面这些是示例，不一定符合你的硬件”
- 但如果只看函数接口和 README，很容易误以为这里的默认表能直接使用

结论：

- 这个文件更适合作为模板，不适合作为默认实现

---

## 3. 当前工程原有问题

### 3.1 原 `LedBar.c` / `LedBar.h` 仍是旧的直驱灯条逻辑

问题：

- 原实现使用 `PA5/PA6/PA7/PB1/PB5` 直接点 5 颗 SOC 灯
- 与 `charlie_595_display_framework` 中的 5 线 Charlie 测试方案完全不是同一套硬件模型

结论：

- 必须整体替换，而不是在旧逻辑上打补丁

---

### 3.2 `APP_LedBar()` 没有挂到主循环周期执行

问题：

- 原工程里 `APP_LedBar()` 只在初始化路径出现
- 主循环没有周期性调用它
- 对 Charlie/595 这种需要持续扫描的显示方案来说，这会导致逻辑无法工作

结论：

- 必须把 `APP_LedBar()` 接回主循环，并利用现有 `b1Sys1msFlag` / `b1Sys100msFlag`

---

## 4. 本次移植采用的硬件映射假设

### 4.1 74HC595 IO

来自：

- `103 + 309/Project/Source/conf/conf_gpio.h`

本次使用：

- `GPIO_LED595_DATA` -> `GPIO_SPI_MOSI / PIN_SPI_MOSI`
- `GPIO_LED595_CLK` -> `GPIO_SPI1_SCK / PIN_SPI1_SCK`
- `GPIO_LED595_LATCH` -> `GPIO_SPI1_NSS / PIN_SPI1_NSS`

说明：

- 我在 `conf_gpio.h` 里增加了 `GPIO_LED595_* / PIN_LED595_*` 别名，避免后续业务代码继续直接写 SPI 名称

---

### 4.2 5 线 Charlie 引脚

来自：

- `103 + 309/Project/Source/charlie_595_display_framework/LedBar.c`

沿用定义：

- `P1 -> PC4`
- `P2 -> PC3`
- `P3 -> PC2`
- `P4 -> PC1`
- `P5 -> PC0`

---

### 4.3 路由转换规则

测试版 `LedBar.c` 的实测路由被转换成正式模型：

- `H1/H2` -> 百位数字 `1` 的两段
- `1A~1G` -> 十位七段
- `2A~2G` -> 个位七段
- `H3` -> 充电图标
- `H4` -> 百分号图标

74HC595 位选约定：

- `Q0` -> 百位 `1`
- `Q1` -> 十位
- `Q2` -> 个位
- `Q3` -> 充电图标
- `Q4` -> 百分号图标

如果后续实物验证发现 595 位序反了，直接改 `LedBar.c` 里的 `LEDBAR_595_SEL_*` 即可。

---

## 5. 软件接入方式

### 5.1 驱动职责

`LedBar.c` 现在承担两层职责：

- 74HC595 位选 bit-bang
- 5 线 Charlie 扫描驱动

### 5.2 调度策略

复用当前工程已有时间基：

- `1ms`：执行 `LedBar_Scan1ms()`
- `100ms`：刷新显示内容

`APP_LedBar()` 现已接入：

- `103 + 309/Project/Source/main.c`

这样不需要新增定时器，也不需要改现有系统节拍。

---

## 6. 当前显示逻辑

当前移植版本实现：

- 显示 `SOC` 数值 `0~100`
- `100` 时显示百位 `1`
- `%` 图标常亮
- 充电时点亮充电图标
- 放电/故障状态会更新 `LedBar_Command`，便于后续扩展显示策略

当前没有把旧灯条时代的“充电流水效果 / 放电特效 / 故障闪烁”硬搬过来，原因是：

- 旧逻辑是针对 5 颗独立灯条写的
- 直接照搬到数码管会产生很多不自然的显示行为

这部分后续如果你有更明确的 UI 规则，可以再继续细化。

---

## 7. 编译验证结果

构建方式：

- `C:\Keil_v5\UV4\UV4.exe`
- 目标：`Target 1`

结果：

- 编译成功
- `0 Error(s), 39 Warning(s)`

说明：

- 这 39 个 warning 全部来自工程原有模块
- 本次新增的 `LedBar.c / LedBar.h / conf_gpio.h / main.c` 改动没有引入新的编译错误

产物：

- `103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.axf`
- `103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.bin`

最新尺寸：

- Code: `44576`
- RO-data: `3036`
- RW-data: `1256`
- ZI-data: `6048`
- ROM Total: `48116` bytes

---

## 8. 后续建议

建议下一步现场确认：

1. 74HC595 的 `Q0~Q4` 与实物位选是否一致
2. `H3/H4` 是否确实分别对应 `充电/%`
3. `100` 时百位“1”的两段方向是否正确
4. 刷新亮度是否满足要求
5. 如果亮度偏低，可考虑进一步提高扫描频率或压缩无效间隔

如果现场验证发现某一段方向反了，优先改 `LedBar.c` 里的 `s_ledbar_routes[]`，不要去改上层显示逻辑。
