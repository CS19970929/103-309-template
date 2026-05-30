# Modbus 参数映射

> 自动生成，请勿手动修改。源数据：`data/examples/param_table.csv`。

| Address | Regs | C name | Name | Access | Description |
|---|---|---|---|---|---|
| `0x2100` | 1 | `cell_ovp_mv` | Cell OVP | rw | 单体过压保护阈值 |
| `0x2101` | 1 | `cell_uvp_mv` | Cell UVP | rw | 单体欠压保护阈值 |
| `0x2200` | 2 | `rated_capacity_mah` | Rated capacity | rw | 额定容量 |
| `0x2400` | 1 | `device_modbus_addr` | Device address | rw | Modbus 从站地址 |
