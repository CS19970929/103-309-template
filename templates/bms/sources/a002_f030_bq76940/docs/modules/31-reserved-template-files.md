# 预留模板与未参与构建文件

## 模块定位

工程中除当前 BMS 主链路外，还保留了一批 Armfly BSP 模板。这些文件大多未被 Keil 工程引用，也没有在 `main.c` 中调用。它们不应被视为当前固件运行功能，但属于项目资产，需要在梳理文档中明确状态。

旧外部 EEPROM 已完全废除，因此 `param.c/.h`、`bsp_i2c_eeprom_24xx.c/.h` 和 `Code/todo.c` 草稿已从模板源删除。

## 文件清单

| 文件 | 状态 | 说明 |
| --- | --- | --- |
| [bsp_beep.c](../../Code/Source/bsp/src/bsp_beep.c) | BSP 模板 | 蜂鸣器控制模板，`bsp.h` 中相关 include 默认注释。当前业务未调用。 |
| [bsp_key.c](../../Code/Source/bsp/src/bsp_key.c) | BSP 模板 | 按键扫描模板，当前业务未通过该模块处理按键。 |
| [bsp_timer.c](../../Code/Source/bsp/src/bsp_timer.c) | BSP 模板 | SysTick/软件定时器模板，当前系统节拍使用 TIM17。 |
| [bsp_i2c_gpio  h7.c](../../Code/Source/bsp/src/bsp_i2c_gpio  h7.c) | BSP 模板 | H7 版本软件 I2C 模板，不适用于当前 STM32F030C8 主路径。 |
| [bsp_i2c_gpio v5.c](../../Code/Source/bsp/src/bsp_i2c_gpio v5.c) | BSP 模板 | 另一版本软件 I2C 模板，不是当前 AFE I2C 主路径。 |

## 与当前主链路的关系

当前运行链路中已经有明确实现：

- 参数存储：使用 [内部 Flash 参数与记录存储](09-eeprom.md)，实际后端固定为内部 Flash。
- 事件日志：使用 [事件日志记录](22-log-record.md)。
- 系统时基：使用 [系统初始化与时基](03-system-init-timebase.md) 中的 TIM17。
- 软件 I2C：AFE 使用 [AFE I2C 驱动](05-afe-i2c-driver.md)，参数存储不使用软件 I2C。
- 蜂鸣器/按键模板：当前没有接入主循环。

## 维护建议

- 不建议直接把这些模板文件接入当前工程，除非先完成接口边界、资源占用和与现有模块的冲突评估。
- 若后续需要保留这些文件作为参考，建议移动到 `docs/reference` 或单独的 `legacy` 目录，减少与当前可编译源码的混淆。
