# BSP 与软件 I2C 基础层

## 相关文件

- [bsp.c](../../Code/Source/bsp/bsp.c)
- [bsp.h](../../Code/Source/bsp/bsp.h)
- [bsp_i2c_gpio.c](../../Code/Source/bsp/src/bsp_i2c_gpio.c)
- [bsp_i2c_gpio.h](../../Code/Source/bsp/inc/bsp_i2c_gpio.h)
- [bsp_i2c_gpio1.c](../../Code/Source/bsp/src/bsp_i2c_gpio1.c)
- [bsp_i2c_gpio1.h](../../Code/Source/bsp/inc/bsp_i2c_gpio1.h)

## 模块职责

BSP 层提供板级基础接口和软件 I2C 实现。当前工程中最重要的实际用途是 AFE 软件 I2C。

## 软件 I2C 实现

| 文件 | 默认 GPIO | 当前用途 |
| --- | --- | --- |
| `bsp_i2c_gpio1.c` | PB10 SCL / PB11 SDA | AFE I2C，当前实际使用。 |
| `bsp_i2c_gpio.c` | PB8 SCL / PB7 SDA | 通用/历史软件 I2C，当前非主路径。 |

## 与业务模块关系

| 模块 | 依赖 |
| --- | --- |
| `I2C_AFE1` | 使用 `bsp_i2c_gpio1` 访问 `BQ769x0`。 |
| 参数存储 | 使用内部 Flash，不走 BSP I2C。 |
| `I2C_Slave` | 预留，不走 BSP 软件 I2C。 |

## 维护建议

- AFE 软件 I2C 属于关键链路，修改延时时序前必须验证 AFE 读写稳定性。
- 如果需要新增软件 I2C 总线，建议明确命名，例如 `bsp_i2c_gpio_afe`、`bsp_i2c_gpio_aux`，避免 `gpio`/`gpio1` 语义不清。
- 通用 BSP 代码中保留了较多模板痕迹，实际资源占用应以调用方为准。
