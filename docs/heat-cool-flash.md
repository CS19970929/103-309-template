# 加热/制冷参数持久化

主 BMS 的 `HEAT_COOL_ELEMENT` 24 个 `UINT16` 仍属于配置双页（`0x0801E000` / `0x0801E400`），与 AFE、保护和 Other 参数共用372字节配置载荷、CRC16、提交标记与回读校验。普通参数修改时使用 `StorageFlash_SaveParameters(0U)`，上电使用 `StorageFlash_InitParameters()` 恢复。

默认不因软件版本字符串变化而清空。需要有意更新加热/制冷默认参数时，设置 `PROJECT_CFG_STORAGE_UPDATE_GENERATION` 为新的非零代次，`PROJECT_CFG_STORAGE_UPDATE_MASK` 包含 bit5（`0x20`）；提交成功后不重复覆盖客户值。

`FD_Release` / `FD_Debug` 当前没有启用旧 `Heat_Cool.c` 控制实现，但加热、制冷参数表与存储保留，不能作为无用历史字段删除。原协议未新增控制寄存器。
