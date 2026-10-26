# Heat_Cool_Element Flash 存储

主 BMS 的 RW 参数现在与 AFE 参数共用 `0x0801E000`、`0x0801E400` 配置双页，地址与掉电流程见 [Flash 参数与事件存储](flash-storage.md)。RW 数据为 290 字节，包括保护参数、`OtherElement`、校准保留字，以及按 `HEAT_COOL_ELEMENT` 字段顺序保存的 24 个 `UINT16`。

按当前产品要求不再读取 242 字节旧格式。启动选择完整有效的新配置；首次初始化由 `HeatCoolElement_Default` 填充，加热参数与其他参数一起参与校验和版本清空。

当前 `FD_Release` 和 `FD_Debug` 未编入旧 `Heat_Cool.c` 控制逻辑。`HEAT_OPEN_CURR` 在当前配置中未定义，默认取 `500U`，与旧控制代码中“50 A 关闭加热”的条件一致。参数可通过 `EEPROM_SaveRWParametersToFlash()` 存储，并在 `InitE2PROM()` 中恢复；本次未增加新的通信寄存器或启用加热控制。
