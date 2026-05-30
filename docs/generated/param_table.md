# 参数表

> 自动生成，请勿手动修改。源数据：`data/param_tables/example_bms_params.csv`。

文档状态：自动生成
维护方式：修改 CSV/JSON 后重新运行 `tools/param_table_check.py` 与生成脚本。

## 字段约定

- `min`、`max`、`default` 为固件侧原始整数值。
- 实际物理值 = 原始整数值 x `scale`，单位见 `unit`。
- `u32`、`s32` 参数占用 2 个连续 Modbus register。

## 参数明细

| Group | Name | C name | Type | Scale | Unit | Min | Max | Default | Access | Save policy | Description |
|---|---|---|---|---|---|---:|---:|---:|---|---|---|
| protection | Cell over voltage threshold | `cell_ovp_mv` | `u16` | 1 | mV | 3000 | 4500 | 4200 | `rw` | `flash` | 单体过压保护阈值 |
| protection | Cell under voltage threshold | `cell_uvp_mv` | `u16` | 1 | mV | 1800 | 3300 | 2500 | `rw` | `flash` | 单体欠压保护阈值 |
| protection | Charge over current threshold | `chg_ocp_dec_a` | `u16` | 0.1 | A | 0 | 2000 | 500 | `rw` | `flash` | 充电过流保护阈值 |
| protection | Discharge over current threshold | `dsg_ocp_dec_a` | `u16` | 0.1 | A | 0 | 3000 | 1000 | `rw` | `flash` | 放电过流保护阈值 |
| soc | Rated capacity | `rated_capacity_mah` | `u32` | 1 | mAh | 1000 | 300000 | 100000 | `rw` | `flash` | 电池包额定容量 |
| soc | SOC low display hold threshold | `soc_low_hold_pct` | `u8` | 1 | % | 0 | 100 | 5 | `rw` | `flash` | SOC 低电量显示保持阈值 |
| soc | SOC full voltage anchor | `soc_full_anchor_mv` | `u16` | 1 | mV | 3300 | 4500 | 4100 | `rw` | `flash` | SOC 满电电压锚点 |
| balance | Balance enable | `balance_enable` | `bool` | 1 | enable | 0 | 1 | 1 | `rw` | `flash` | 均衡功能使能开关 |
| balance | Balance start voltage | `balance_start_mv` | `u16` | 1 | mV | 3000 | 4300 | 3600 | `rw` | `flash` | 均衡启动单体电压阈值 |
| system | Device Modbus address | `device_modbus_addr` | `u8` | 1 | addr | 1 | 247 | 1 | `rw` | `flash` | 设备 Modbus 从站地址 |
| system | Aging duration | `aging_duration_hours` | `u16` | 1 | h | 0 | 2000 | 72 | `rw` | `factory` | 出厂老化目标时长 |
| runtime | Pack voltage monitor | `pack_voltage_mv` | `u16` | 1 | mV | 0 | 65535 | 0 | `ro` | `none` | 实时电池包总压只读值 |

## 默认值物理量视图

| C name | Default raw | Default physical | Unit |
|---|---:|---:|---|
| `cell_ovp_mv` | 4200 | 4200 | mV |
| `cell_uvp_mv` | 2500 | 2500 | mV |
| `chg_ocp_dec_a` | 500 | 50 | A |
| `dsg_ocp_dec_a` | 1000 | 100 | A |
| `rated_capacity_mah` | 100000 | 100000 | mAh |
| `soc_low_hold_pct` | 5 | 5 | % |
| `soc_full_anchor_mv` | 4100 | 4100 | mV |
| `balance_enable` | 1 | 1 | enable |
| `balance_start_mv` | 3600 | 3600 | mV |
| `device_modbus_addr` | 1 | 1 | addr |
| `aging_duration_hours` | 72 | 72 | h |
| `pack_voltage_mv` | 0 | 0 | mV |
