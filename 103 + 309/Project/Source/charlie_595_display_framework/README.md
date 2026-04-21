# 74HC595 + 5线 Charlieplexing 数码管显示框架

本框架适用于：

- **74HC595 提供选通信号**
- **5 个 MCU GPIO 作为 Charlieplexing 驱动线**
- 显示器结构类似：
  - 百分位：只显示 `1` 或不显示
  - 十分位：7 段
  - 个位：7 段
  - 小挂件：`充电`、`%`

> 由于你的实物接线未给出，本框架采用**“可配置映射表”**方式。
> 你只需要修改：
>
> 1. `74HC595` 底层引脚驱动
> 2. `5根GPIO` 读写/方向控制
> 3. `display_hw_map.c` 中的 **LED 映射表**

---

## 目录结构

```text
charlie_595_display_framework/
├─ inc/
│  ├─ display_74hc595.h
│  ├─ display_charlie.h
│  ├─ display_segmap.h
│  ├─ display_types.h
│  └─ display_hw_map.h
├─ src/
│  ├─ display_74hc595.c
│  ├─ display_charlie.c
│  ├─ display_segmap.c
│  └─ display_hw_map.c
├─ example/
│  └─ main_example.c
├─ docs/
│  ├─ 01_框架设计说明.md
│  ├─ 02_移植说明.md
│  ├─ 03_接线映射填写说明.md
│  ├─ 04_测试说明.md
│  └─ 05_变更记录.md
└─ README.md
```

---

## 你需要改的地方

### 1. 改 74HC595 底层

文件：

- `src/display_74hc595.c`

需要你实现这些底层函数：

- `disp595_hw_set_data()`
- `disp595_hw_set_clk()`
- `disp595_hw_set_latch()`
- `disp595_hw_delay_small()`

---

### 2. 改 5 根 GPIO 控制

文件：

- `src/display_charlie.c`

需要你实现这些底层函数：

- `charlie_hw_pin_mode(pin, mode)`
- `charlie_hw_pin_write(pin, level)`

---

### 3. 填写 LED 映射表（最关键）

文件：

- `src/display_hw_map.c`

这里定义了：

- 百分位 `1`
- 十分位 a~g
- 个位 a~g
- 充电图标
- 百分号图标

每个 LED 都要填：
- 属于哪个 `74HC595 位选`
- 属于哪两个 Charlie 线之间
- 正向点亮时的方向

---

## 调用方式

### 初始化

```c
display_init();
```

### 周期扫描（建议 1ms 调一次）

```c
display_scan_task_1ms();
```

### 设置显示内容

```c
display_set_value(87, 1, 1); // 87%，充电图标开，百分号开
```

说明：

- `87` -> 十位显示 `8`，个位显示 `7`
- 百分位是否显示 `1` 由逻辑自动决定：`value >= 100` 时显示 `1`
- `charge_on`：充电图标
- `percent_on`：百分号图标

---

## 推荐扫描策略

- `display_scan_task_1ms()` 每 1ms 调用一次
- 每次只点亮 **1 个 LED**
- 通过视觉暂留形成整体显示
- 如果亮度不足，可：
  - 提高扫描频率
  - 提高单点占空比
  - 减少同时显示元素数量
  - 优化限流电阻与驱动能力

---

## 特别说明

Charlieplexing 的核心是：

- 1 个时刻只允许**一个 LED 通路**
- 其余引脚必须高阻
- 否则容易串亮、鬼影

本框架在软件架构上已经把这些动作拆开，适合你直接移植到 STM32 / Telink / 其他 MCU。

---
