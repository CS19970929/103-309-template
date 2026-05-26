# fd_T3Max_D009 CAN 通信适配说明

## 适配范围

- BMS App 增加 CAN App 服务标准帧：请求 `0x60`，响应 `0x61`。
- 已接入命令：状态读取、进入 IAP、单寄存器读、单寄存器写、连续块读。
- 寄存器读写复用 `Sci_Upper.c` 原有 Modbus 读写校验、权限、范围和副作用处理，不新增独立参数表。
- comm tool 上位机和 F103RET6 固件工程从当前分支同步，UART 默认使用 `CT_COMM_UART_PORT_USART1`，后续只改 `CT_COMM_UART_PORT` 即可切换串口。
- 用户版上位机固定输出为 `dist\BMS_CommTool_Upgrade_UI.exe`，不要生成其它名字的 exe。

## 未适配项

- D009 分支没有 `FactoryAging` 模块，本次不迁移老化模式开启、关闭、重置时间逻辑。
- `test_Autocurrent_cycle()` 保留，不在本次 CAN 适配中删除或改名。

## 升级安全

- BMS App 正常起始地址仍为 `0x08004800`。
- 当前实物使用的 BMS IAP 工程固定为 `E:\work\a002\new 030\IAP 103CB`，不要再按其它临时 IAP 工程判断协议。
- 该 IAP 与 `can-upgrade-host` 分支一致，进入 IAP 的门闩是 SRAM mailbox `0x20004FE0`，字段为 `magic=0x49415031`、`request=0x5AA55AA5`、`crc=magic ^ request ^ 0xA5A55A5A`。
- App RAM 必须预留 `0x20004FE0-0x20004FFF`，Keil IRAM 长度固定为 `0x00004FE0`；禁止改回 `0x00005000`，否则 mailbox 可能被栈或变量覆盖。
- 串口 `0xFFFD` 和 CAN App `ENTER_IAP` 都必须调用 `AppUpgrade_RequestIap()` 写 SRAM mailbox，再延时复位。`FLASH_ADDR_UPDATE_FLAG/FLASH_TO_IAP_VALUE` 只保留为旧协议兼容常量，不再作为实际 IAP 进入门闩。
- CAN-IAP 协议仍为扩展帧 `0x14F8F000/0x14F8F100/0x14000000`，节点默认 `1`，与 `E:\work\a002\new 030\IAP 103CB\Include\can_iap_protocol.h` 保持一致。
- 如果 comm tool 日志停在 `state=3 error=0x02 written=0 expect_seq=0`，优先确认 App 是否写入 SRAM mailbox 并真正停在 IAP；这是 HELLO 阶段无 ACK，不是数据块或 CRC 问题。
- 禁止把 App bin 裸写到 `0x08000000`，烧录 App 仍使用仓库安全脚本。
