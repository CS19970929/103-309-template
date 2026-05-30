# 工具清单

| 工具 | 作用 | 示例数据 | README 说明 |
|---|---|---|---|
| `embedded_project_doctor.py` | 扫描 C/H 文件，输出函数、中断、全局变量、volatile、TODO、while 死等、delay、HAL、Flash、MOS、风险函数、超长函数、宏统计 | `data/examples/example_c_project/` | 本文、`docs/01_usage.md` |
| `map_analyze.py` | 解析 Keil map，输出 Flash/RAM、大函数、大变量、模块占用 | `data/examples/example_keil.map` | 本文、`docs/03_data_format.md` |
| `map_diff.py` | 对比两份 map，输出区域和符号变化 | `example_keil_old.map`、`example_keil.map` | 本文 |
| `modbus_cli.py` | Modbus RTU 读写调试，写操作默认 dry-run | `--demo` | 本文、`docs/04_safety_rules.md` |
| `modbus_param_dump.py` | 按 CSV 寄存器表导出参数 | `modbus_registers.csv`、`modbus_values_a.csv` | 本文 |
| `modbus_param_diff.py` | 对比两份参数 dump | `modbus_dump_a.csv`、`modbus_dump_b.csv` | 本文 |
| `can_decode.py` | 按 JSON 协议离线解码 CAN 日志 | `can_protocol.json`、`can_log.txt` | 本文 |
| `can_log_analyze.py` | 分析 CAN ID 帧数和周期 | `can_log.txt` | 本文 |
| `bms_log_decode.py` | 按事件码 JSON 解码 BMS 事件日志 | `bms_events.json`、`bms_event_log.csv` | 本文 |
| `bms_event_report.py` | 统计 BMS 事件等级和次数 | `bms_events.json`、`bms_event_log.csv` | 本文 |
| `param_table_check.py` | 检查参数表地址、类型、默认值、权限、保存策略 | `param_table.csv` | 本文 |
| `param_code_gen.py` | 生成 `param_table.h`、`param_default.c`、`modbus_param_map.c` | `param_table.csv` | 本文 |
| `param_doc_gen.py` | 生成参数表和 Modbus 映射 Markdown | `param_table.csv` | 本文 |
| `protection_sim.py` | BMS 保护逻辑 PC 仿真 | `protection_params.json`、`protection_timeseries.csv` | 本文 |
| `soc_sim.py` | SOC 安时积分 PC 仿真 | `soc_params.json`、`soc_timeseries.csv` | 本文 |
| `fault_decode.py` | Cortex-M HardFault 寄存器和 PC/LR 符号解析 | `fault_snapshot.json`、`example_keil.map` | 本文 |
| `change_log_gen.py` | 根据本地 git diff 生成嵌入式变更记录 | 当前 git 仓库 | 本文 |

所有工具均支持：

```bash
python3 tools/<tool>.py --help
```
