# LedBar 运行时状态收口说明

## 背景

`LedBar.c` 原先使用二十多个 `s_ledbar_*` 文件静态变量分别保存初始化状态、休眠状态、扫描帧、按键去抖、MCU_WK 去抖和充电显示去抖。变量数量多，后续维护时容易漏改初始化和复位路径。

## 本次调整

- 新增 `LedBarRuntime`，把 LedBar 文件内运行时状态集中到 `s_ledbar_runtime`。
- 保留原 `s_ledbar_*` 名称作为 `LedBar.c` 内部兼容别名，当前调用点不需要大面积改动。
- 不修改 `LedBar.h` 对外接口。
- 不修改 BKP 寄存器、GPIO 路由表、扫描定时器配置、长按休眠逻辑和 SOC 显示策略。

## 行为边界

本次只是状态变量收口，不改变以下行为：

- 查理复用 routes 和 GPIO pin 映射。
- TIM4 扫描中断路径。
- `LedBar_SaveSleepSoc` / `LedBar_LoadSleepSoc` 的 BKP 数据格式。
- `APP_LedBar` 的显示请求、充电图标、故障显示和休眠判断。
- `LEDBAR_SLEEP_ENABLE`、`LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST` 等构建开关。

## 后续建议

后续可在单独提交里逐步把本文件内的兼容别名替换为 `s_ledbar_runtime.xxx` 直接访问。替换时仍应保持每次只改一个模块，并通过编译和 `tools/project_check.py` 确认无回归。
