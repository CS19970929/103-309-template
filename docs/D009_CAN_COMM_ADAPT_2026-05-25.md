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
- CAN 进入 IAP 只写原有 `FLASH_ADDR_UPDATE_FLAG`，延时复位后由旧 IAP 入口处理。
- 禁止把 App bin 裸写到 `0x08000000`，烧录 App 仍使用仓库安全脚本。
