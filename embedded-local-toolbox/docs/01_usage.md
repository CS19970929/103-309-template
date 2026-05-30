# 使用说明

## 环境要求

- Python 3.9+。
- 默认不需要第三方依赖。
- 串口 Modbus 真实读写需要 `pyserial`。
- CAN 第一阶段只做离线日志解析，不需要 `python-can`。

## 离线自测

在 `embedded-local-toolbox/` 目录运行：

```bash
python3 tools/embedded_project_doctor.py data/examples/example_c_project --out reports/project_doctor.md --force
python3 tools/map_analyze.py data/examples/example_keil.map --out reports/map_analyze.md --force
python3 tools/map_diff.py data/examples/example_keil_old.map data/examples/example_keil.map --out reports/map_diff.md --force
python3 tools/modbus_cli.py --demo --read 0x2100 2 --out reports/modbus_cli.md --force
python3 tools/modbus_param_dump.py --csv-out generated/modbus_dump.csv --out reports/modbus_param_dump.md --force
python3 tools/modbus_param_diff.py data/examples/modbus_dump_a.csv data/examples/modbus_dump_b.csv --out reports/modbus_param_diff.md --force
python3 tools/can_decode.py --out reports/can_decode.md --force
python3 tools/can_log_analyze.py --out reports/can_log_analyze.md --force
python3 tools/bms_log_decode.py --out reports/bms_log_decode.md --force
python3 tools/bms_event_report.py --out reports/bms_event_report.md --force
python3 tools/param_table_check.py data/examples/param_table.csv
python3 tools/param_code_gen.py data/examples/param_table.csv --out-dir generated --force
python3 tools/param_doc_gen.py data/examples/param_table.csv --out-dir generated --force
python3 tools/protection_sim.py --out reports/protection_sim.md --force
python3 tools/soc_sim.py --csv-out generated/soc_sim.csv --out reports/soc_sim.md --force
python3 tools/fault_decode.py --out reports/fault_decode.md --force
python3 tools/change_log_gen.py --repo .. --base HEAD --out reports/change_log.md --force
```

如报告已存在，需要显式加 `--force` 才会覆盖。

## 迁移到具体工程

1. 只把目标工程路径、map 文件、CSV/JSON 配置作为输入。
2. 不让工具自动改源码。
3. 每次生成 Markdown 报告后先人工确认。
4. 对 Flash、MOS、通信写入、IAP、参数区相关结论必须回到板端验证。
