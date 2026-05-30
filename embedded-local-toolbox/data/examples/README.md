# 示例数据说明

本目录为 `embedded-local-toolbox` 第一阶段工具提供离线自测数据，不依赖硬件、不联网。

| 文件/目录 | 用途 |
|---|---|
| `example_c_project/` | `embedded_project_doctor.py` 的 C 项目扫描样例 |
| `example_keil.map` / `example_keil_old.map` | `map_analyze.py` 与 `map_diff.py` 样例 |
| `modbus_registers.csv` / `modbus_values_*.csv` / `modbus_dump_*.csv` | Modbus 参数导出和对比样例 |
| `can_protocol.json` / `can_log.txt` | CAN 离线解码和周期分析样例 |
| `serial_live_log.txt` | 串口监控、关键字标记和 Modbus RTU 帧识别样例 |
| `bms_dashboard_registers.csv` / `bms_dashboard_values.csv` | BMS 命令行 dashboard 离线样例 |
| `openocd_probe_output.txt` / `stlink_flash_size_output.txt` | OpenOCD/STLink 只读探测离线输出样例 |
| `firmware_demo.bin` / `firmware_demo.hex` | 固件发布报告样例产物 |
| `bms_events.json` / `bms_event_log.csv` | BMS 事件日志解析样例 |
| `param_table.csv` | 参数表检查、代码生成、文档生成样例 |
| `protection_params.json` / `protection_timeseries.csv` | 保护逻辑仿真样例 |
| `soc_params.json` / `soc_timeseries.csv` | SOC 仿真样例 |
| `fault_snapshot.json` | HardFault 解析样例 |
