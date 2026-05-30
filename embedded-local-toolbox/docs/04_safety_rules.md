# 安全规则

## 总原则

- 工具只在本机运行。
- 不联网。
- 不上传任何项目数据。
- 不自动删除文件。
- 不自动改源码。
- 不自动覆盖报告或生成文件，覆盖必须显式加 `--force`。
- 所有报告使用 Markdown，便于审查和归档。
- 硬件连接必须由命令行参数显式触发。

## 危险操作

### Modbus 写操作

`modbus_cli.py --write` 默认 dry-run，只输出计划写入地址和值。

必须显式加：

```bash
--execute
```

才会尝试真实串口写入。

### 生成文件

`param_code_gen.py` 和 `param_doc_gen.py` 会生成文件，但不会覆盖已有文件，除非加 `--force`。

生成文件带“自动生成，请勿手动修改”说明。

### Git diff

`change_log_gen.py` 只读取本地 git diff，不执行 commit、reset、checkout、clean、push。

### 串口监控

`serial_live_monitor.py` 不传 `--port` 时只读取示例日志。传入 `--port` 后只打开串口读取数据，不发送命令。

`bms_live_dashboard.py` 传入 `--port` 后会周期读取 Modbus holding registers，不执行写寄存器。

### OpenOCD/STLink

`openocd_probe.py` 和 `stlink_flash_size_check.py` 默认不访问硬件。只有显式传入 `--connect` 才会调用 OpenOCD。

当前实现只允许以下只读意图：

- 检查 OpenOCD 是否安装。
- 连接 STLink。
- 读取 `DBGMCU_IDCODE`。
- 读取 STM32 flash size 系统存储器地址。

工具命令不包含：

- `erase`
- `program`
- `flash write`
- `reset`
- `reset halt`
- `unlock`
- `lock`

如果后续新增任何擦写、解锁、复位或寄存器写入功能，必须增加二次确认参数，并在报告中记录确认信息。

## 嵌入式人工复核清单

涉及以下内容时，工具报告只能作为线索，必须人工复核并做板端验证：

- Flash/EEPROM/IAP 写入。
- MOS/充放电开关控制。
- UART/CAN/I2C/SPI 中断和 DMA。
- 低功耗 STOP/STANDBY 唤醒。
- Modbus/CAN 对外协议地址和帧格式。
- SOC、保护参数、校准参数和量产配置。
