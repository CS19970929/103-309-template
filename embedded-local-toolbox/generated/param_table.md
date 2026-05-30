# 参数表

> 自动生成，请勿手动修改。源数据：`data/examples/param_table.csv`。

| Group | Name | C name | Type | Scale | Unit | Min | Max | Default | Access | Save | Description |
|---|---|---|---|---|---|---|---|---|---|---|---|
| protection | Cell OVP | `cell_ovp_mv` | u16 | 1 | mV | 3000 | 4500 | 4200 | rw | flash | 单体过压保护阈值 |
| protection | Cell UVP | `cell_uvp_mv` | u16 | 1 | mV | 1800 | 3300 | 2500 | rw | flash | 单体欠压保护阈值 |
| soc | Rated capacity | `rated_capacity_mah` | u32 | 1 | mAh | 1000 | 300000 | 100000 | rw | flash | 额定容量 |
| system | Device address | `device_modbus_addr` | u8 | 1 | addr | 1 | 247 | 1 | rw | flash | Modbus 从站地址 |
