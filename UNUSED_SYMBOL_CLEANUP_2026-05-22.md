# 无效宏、变量和函数清理说明

## 结论

本次清理删除的是已经没有有效调用方、没有硬件输出、或只做参数范围判断但不改变任何状态的旧符号。清理目标是减少误用入口和无效分支，同时保留对外协议地址、事件编号和关键结构体位序里的空洞。

## 清理范围

- 删除 Heat/Cool 模块移除后残留的状态位写入、错误分支、日志事件和 RTC 休眠阻塞原因。
- 删除 `0x1100` / `0x1101` 两个无动作开关命令入口，保留地址空洞为 reserved。
- 限制 `0x1102` / `0x1103` 系统功能开关只接受仍有意义的功能 ID，拒绝 Heat/Cool 和其他无效 ID。
- 删除空实现和旧测试入口：`InitE2PROM_i2c`、`App_E2promDeal`、`EEPROM_test`、`DataLoad_CellVolt_Test`、`test_Autocurrent_cycle`、`FlashTest`。
- 删除旧 AFE/I2C 调试函数和变量：`AFE_IDLE_Old`、`AFE_GetData`、`TwiWrite_old`、`TwiRead_old`、`gu8_DriverStartUpFlag`、`aaaaaa1` 等。
- 删除软件奇偶校验串口帧生成函数：`OddEven_Check`、`Usart_9bitOddEvenData_Frame`。当前 USART 使用硬件配置，不需要该路径。
- 删除旧外部 EEPROM 地址数组和历史标志宏，只保留仍被故障记录、校准参数和系统功能位使用的地址。
- 删除 `SleepDeal.c` 中未被调用的测试睡眠模式包装函数。

## 兼容性处理

- `LogRecord.h` 中原 Heat/Cool 事件编号没有挤压，改名为 `EVENT_RESERVED_4` / `EVENT_RESERVED_5`。
- `rtc_sleep.h` 中原 Heat 阻塞原因编号没有挤压，改名为 `LOW_POWER_RTC_BLOCK_RESERVED_2`。
- `System_Monitor.h` 和 `Can_HDX.h` 中已删除业务含义的 bit 位改名为 reserved，保持结构体位序不变。
- `Sci_Upper.h` 中 `0x1100` / `0x1101` 改为 `RS485_CMD_ADDR_RESERVED_1100` / `RS485_CMD_ADDR_RESERVED_1101`，因此 `RS485_CMD_ADDR_SYSTEM_FUNCTION_ON` 仍是 `0x1102`，`RS485_CMD_ADDR_SYSTEM_FUNCTION_OFF` 仍是 `0x1103`。
- 当前清理不改变 App 烧录地址和 Flash 参数主结构，不影响 `0x08004800` App 安全烧录规则。

## 自动守卫

`tools/project_check.py` 已加入删除守卫，检查 C/H 源码中不再出现本次删除的旧符号，包括 Heat/Cool 残留、无动作命令、旧 EEPROM 宏、旧 AFE/I2C 测试函数、`TwiWrite_old` / `TwiRead_old` 等。

后续如果确实需要恢复某个功能，应先重新定义协议、硬件输出和测试覆盖，再移除对应 guard，而不是直接把旧符号加回来。
