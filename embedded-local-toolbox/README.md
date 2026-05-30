# embedded-local-toolbox

`embedded-local-toolbox` 是一个本地嵌入式长期工具箱，不绑定单个固件项目。目标场景是 STM32F0/F1/F4、裸机、标准库、BMS 保护板项目，也可用于普通嵌入式 C 项目。

## 设计原则

- 本地运行，不联网，不上传项目数据。
- Python 3 优先，CSV/JSON/Markdown 优先。
- 默认只使用 Python 标准库；串口工具需要真实硬件时可安装 `pyserial`。
- CAN 工具必须支持离线日志解析，不强依赖 `python-can`。
- 所有工具支持 `--help`。
- 所有报告输出 Markdown。
- 危险操作默认 dry-run，不自动删除、不自动覆盖、不自动改源码。
- 硬件调试工具默认只读；OpenOCD/STLink 工具不包含烧录、擦除、复位命令。

## 目录

| 目录 | 用途 |
|---|---|
| `tools/` | CLI 工具脚本 |
| `data/examples/` | 每个工具的离线示例数据 |
| `generated/` | 代码/文档生成输出目录 |
| `reports/` | 推荐报告输出目录 |
| `docs/` | 使用说明、格式说明、安全规则和路线图 |

## 典型使用流程

1. 查看工具清单：

```bash
python3 tools/embedded_project_doctor.py --help
python3 tools/map_analyze.py --help
python3 tools/modbus_cli.py --help
```

2. 先用示例数据离线跑通：

```bash
python3 tools/embedded_project_doctor.py data/examples/example_c_project --out reports/project_doctor.md --force
python3 tools/map_analyze.py data/examples/example_keil.map --out reports/map_analyze.md --force
python3 tools/can_decode.py --out reports/can_decode.md --force
python3 tools/serial_live_monitor.py --md-out reports/serial_live_monitor.md --force
python3 tools/openocd_probe.py --out reports/openocd_probe.md --force
python3 tools/firmware_artifact_report.py --out reports/firmware_artifact_report.md --force
```

3. 将自己的项目数据复制为 CSV/JSON 输入，不直接修改源码。

4. 输出 Markdown 报告到 `reports/`，人工审查后再决定是否进入项目变更。

说明：工具默认不覆盖已有文件；重复运行示例时需要显式加 `--force`。

真实硬件连接示例：

```bash
python3 tools/serial_live_monitor.py --port COM4 --baud 19200 --duration 10 --md-out reports/serial_live_monitor.md
python3 tools/bms_live_dashboard.py --port COM4 --baud 19200 --slave 1 --cycles 10
python3 tools/openocd_probe.py --connect --target target/stm32f1x.cfg --out reports/openocd_probe.md
```

这些命令仍然不执行烧录、擦除或源码修改；涉及真实串口/探针连接前，请先确认端口和目标板供电状态。

## 第一阶段工具

详见 [docs/02_tool_list.md](docs/02_tool_list.md)。

## 数据格式

详见 [docs/03_data_format.md](docs/03_data_format.md)。

## 安全规则

详见 [docs/04_safety_rules.md](docs/04_safety_rules.md)。
