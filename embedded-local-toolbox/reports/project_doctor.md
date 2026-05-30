# 嵌入式项目静态风险报告

## 文件统计

| 类型 | 数量 |
|---|---|
| .c | 2 |
| 总行数 | 74 |

## 函数列表

| 函数 | 文件 | 起始行 | 估算行数 |
|---|---|---|---|
| SPI1_IRQHandler | src/driver.c | 4 | 4 |
| Board_LedSet | src/driver.c | 8 | 5 |
| USART1_IRQHandler | src/main.c | 16 | 5 |
| CAN1_RX0_IRQHandler | src/main.c | 21 | 5 |
| App_WaitReady | src/main.c | 26 | 6 |
| App_SaveParam | src/main.c | 32 | 5 |
| App_CopyName | src/main.c | 37 | 6 |
| App_MosControl | src/main.c | 43 | 9 |
| main | src/main.c | 52 | 11 |

## 中断函数

| 文件 | 行 | 对象 | 说明 |
|---|---|---|---|
| src/driver.c | - | SPI1_IRQHandler | SPI |
| src/main.c | - | USART1_IRQHandler | USART |
| src/main.c | - | CAN1_RX0_IRQHandler | CAN |

## 全局变量候选

| 文件 | 行 | 对象 | 说明 |
|---|---|---|---|
| src/main.c | - | g_ms_tick | 需人工确认作用域 |
| src/main.c | - | g_mos_state | 需人工确认作用域 |
| src/main.c | - | s_pack_voltage_mv | 需人工确认作用域 |

## volatile 变量

| 文件 | 行 | 对象 | 说明 |
|---|---|---|---|
| src/main.c | - | g_ms_tick | 可能跨中断/主循环共享 |

## TODO/FIXME

| 文件 | 行 | 对象 | 说明 |
|---|---|---|---|
| src/main.c | 24 | - | /* TODO: decode CAN frame */ |

## while 死等

| 文件 | 行 | 对象 | 说明 |
|---|---|---|---|
| src/main.c | 29 | - | while ((g_ms_tick & 0x01U) == 0U) { |

## delay 调用

| 文件 | 行 | 对象 | 说明 |
|---|---|---|---|
| src/main.c | 59 | - | DelayMs(10); |

## HAL 调用

| 文件 | 行 | 对象 | 说明 |
|---|---|---|---|
| src/driver.c | 3 | - | void HAL_GPIO_WritePin(int port, int pin, int value); |
| src/driver.c | 11 | - | HAL_GPIO_WritePin(0, 1, on); |

## Flash 写入调用

| 文件 | 行 | 对象 | 说明 |
|---|---|---|---|
| src/main.c | 35 | - | Flash_WriteParam(0x2200, s_pack_voltage_mv); |

## MOS 控制相关

| 文件 | 行 | 对象 | 说明 |
|---|---|---|---|
| src/main.c | 7 | - | #define MOS_CHG_ON()  do { g_mos_state \|= 0x01; } while (0) |
| src/main.c | 8 | - | #define MOS_DSG_OFF() do { g_mos_state &= (uint8_t)~0x02; } while (0) |
| src/main.c | 47 | - | MOS_CHG_ON(); |
| src/main.c | 49 | - | MOS_DSG_OFF(); |

## 风险库函数

| 文件 | 行 | 对象 | 说明 |
|---|---|---|---|
| src/main.c | 40 | - | strcpy(dst, src); |
| src/main.c | 41 | - | sprintf(dst, "BMS-%s", src); |
| src/main.c | 56 | - | memset(name, 0, sizeof(name)); |

## 超长函数

未发现。

## UART/CAN/I2C/SPI 中断函数

| 文件 | 行 | 对象 | 说明 |
|---|---|---|---|
| src/driver.c | - | SPI1_IRQHandler | SPI |
| src/main.c | - | USART1_IRQHandler | USART |
| src/main.c | - | CAN1_RX0_IRQHandler | CAN |

## 宏开关统计

| 宏 | 出现次数 |
|---|---|
| PROJECT_CFG_BMS_ENABLE | 1 |
| PROJECT_CFG_SOC_ENABLE | 1 |
| MOS_CHG_ON | 1 |
| MOS_DSG_OFF | 1 |
