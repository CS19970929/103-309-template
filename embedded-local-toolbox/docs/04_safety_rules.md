# 安全规则

## 总原则

- 工具只在本机运行。
- 不联网。
- 不上传任何项目数据。
- 不自动删除文件。
- 不自动改源码。
- 不自动覆盖报告或生成文件，覆盖必须显式加 `--force`。
- 所有报告使用 Markdown，便于审查和归档。

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

## 嵌入式人工复核清单

涉及以下内容时，工具报告只能作为线索，必须人工复核并做板端验证：

- Flash/EEPROM/IAP 写入。
- MOS/充放电开关控制。
- UART/CAN/I2C/SPI 中断和 DMA。
- 低功耗 STOP/STANDBY 唤醒。
- Modbus/CAN 对外协议地址和帧格式。
- SOC、保护参数、校准参数和量产配置。
