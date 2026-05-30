# Modbus 参数寄存器映射

> 自动生成，请勿手动修改。源数据：`data/param_tables/example_bms_params.csv`。

文档状态：自动生成
地址规则：每个参数的起始地址来自 `modbus_addr`，`u32/s32` 占用 2 个连续 register。

| Address | Register count | C name | Name | Type | Access | Save policy | Description |
|---:|---:|---|---|---|---|---|---|
| `0x2100` | 1 | `cell_ovp_mv` | Cell over voltage threshold | `u16` | `rw` | `flash` | 单体过压保护阈值 |
| `0x2101` | 1 | `cell_uvp_mv` | Cell under voltage threshold | `u16` | `rw` | `flash` | 单体欠压保护阈值 |
| `0x2102` | 1 | `chg_ocp_dec_a` | Charge over current threshold | `u16` | `rw` | `flash` | 充电过流保护阈值 |
| `0x2103` | 1 | `dsg_ocp_dec_a` | Discharge over current threshold | `u16` | `rw` | `flash` | 放电过流保护阈值 |
| `0x2200` | 2 | `rated_capacity_mah` | Rated capacity | `u32` | `rw` | `flash` | 电池包额定容量 |
| `0x2202` | 1 | `soc_low_hold_pct` | SOC low display hold threshold | `u8` | `rw` | `flash` | SOC 低电量显示保持阈值 |
| `0x2203` | 1 | `soc_full_anchor_mv` | SOC full voltage anchor | `u16` | `rw` | `flash` | SOC 满电电压锚点 |
| `0x2300` | 1 | `balance_enable` | Balance enable | `bool` | `rw` | `flash` | 均衡功能使能开关 |
| `0x2301` | 1 | `balance_start_mv` | Balance start voltage | `u16` | `rw` | `flash` | 均衡启动单体电压阈值 |
| `0x2400` | 1 | `device_modbus_addr` | Device Modbus address | `u8` | `rw` | `flash` | 设备 Modbus 从站地址 |
| `0x2401` | 1 | `aging_duration_hours` | Aging duration | `u16` | `rw` | `factory` | 出厂老化目标时长 |
| `0xD000` | 1 | `pack_voltage_mv` | Pack voltage monitor | `u16` | `ro` | `none` | 实时电池包总压只读值 |
