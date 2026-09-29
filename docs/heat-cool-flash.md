# Heat_Cool_Element Flash 存储

主 BMS 的 RW 参数仍使用 `0x0801C400` 和 `0x0801CC00` 双 slot。记录头、magic、版本号和原有保护参数、`OtherElement`、校准保留字的顺序不变；末尾新增 24 个 `UINT16`，按 `HEAT_COOL_ELEMENT` 字段顺序保存。payload 从 242 字节变为 290 字节，CRC16/Modbus 覆盖完整 payload。

启动时按 sequence 选取最新的有效 slot，兼容 242 字节旧记录。读取旧记录时，`Heat_Cool_Element` 使用 `HeatCoolElement_Default`，其他参数按旧记录恢复。首次写入新记录时选取最新旧 slot 的另一页，写入和校验成功后才继续使用新格式；原双 slot 地址与擦除页大小不变。

当前 `FD_Release` 和 `FD_Debug` 未编入旧 `Heat_Cool.c` 控制逻辑。`HEAT_OPEN_CURR` 在当前配置中未定义，默认取 `500U`，与旧控制代码中“50 A 关闭加热”的条件一致。参数可通过 `EEPROM_SaveRWParametersToFlash()` 存储，并在 `InitE2PROM()` 中恢复；本次未增加新的通信寄存器或启用加热控制。
