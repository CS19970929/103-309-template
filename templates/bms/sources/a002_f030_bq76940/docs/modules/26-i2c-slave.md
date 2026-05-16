# I2C Slave 预留模块

## 相关文件

- [I2C_Slave.c](../../Code/Source/I2C_Slave.c)
- [I2C_Slave.h](../../Code/Include/I2C_Slave.h)

## 模块状态

该模块源码存在并参与构建，但当前属于预留骨架：

- `Init_I2CSlaver()` 为空。
- `I2C_Salve_Deal()` 为空。
- `main.c` 中未调用该模块。

## 设计意图

从命名和缓冲区结构看，该模块可能用于将 BMS 数据打包为 I2C/SMBus 从机数据，供外部主机读取。代码中存在 D0/D1 类数据缓冲概念，但尚未形成完整协议。

## 当前风险

- 没有硬件资源初始化，不能认为 I2C Slave 功能可用。
- 没有中断处理和地址配置。
- 没有与 `g_stCellInfoReport` 的稳定数据同步策略。

## 维护建议

- 如需实现 I2C/SMBus 从机，应明确 I2C 外设、GPIO、从机地址、时序、PEC/CRC、主机读写模型。
- 建议先补协议文档，再实现初始化、中断、缓冲区快照和异常恢复。
- 不应复用 AFE 软件 I2C 引脚作为从机总线；旧 EEPROM I2C 已废除，PB3/PB4 若重新使用必须先更新资源表。
