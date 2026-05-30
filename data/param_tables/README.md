# BMS 参数表生成工具说明

本文说明如何用 CSV/JSON 统一维护 BMS 参数表，并生成固件 C 文件与 Modbus 文档。

## 目录约定

- 参数源文件：`data/param_tables/*.csv` 或 `data/param_tables/*.json`
- 检查脚本：`tools/param_table_check.py`
- C 生成脚本：`tools/param_code_gen.py`
- 文档生成脚本：`tools/param_doc_gen.py`
- C 输出目录：`generated/`
- 文档输出目录：`docs/generated/`

## CSV 字段

| 字段 | 说明 |
|---|---|
| `group` | 参数分组，例如 `protection`、`soc`、`system` |
| `name` | 面向文档的人类可读名称 |
| `c_name` | 固件侧 C 标识符，必须唯一 |
| `modbus_addr` | 起始 Modbus register 地址，支持 `0x` 十六进制 |
| `data_type` | 数据类型，支持 `bool`、`u8`、`s8`、`u16`、`s16`、`u32`、`s32` |
| `scale` | 原始整数值到物理值的比例，物理值 = raw x scale |
| `unit` | 物理单位 |
| `min` | 固件侧原始最小值 |
| `max` | 固件侧原始最大值 |
| `default` | 固件侧原始默认值 |
| `access` | 访问权限，支持 `ro`、`rw`、`wo` |
| `save_policy` | 保存策略，支持 `none`、`runtime`、`flash`、`eeprom`、`nvm`、`factory` |
| `description` | 参数说明，不能为空 |

## 常用命令

检查示例参数表：

```bash
python3 tools/param_table_check.py data/param_tables/example_bms_params.csv
```

生成 C 文件：

```bash
python3 tools/param_code_gen.py data/param_tables/example_bms_params.csv
```

生成 Markdown 文档：

```bash
python3 tools/param_doc_gen.py data/param_tables/example_bms_params.csv
```

查看合法枚举值：

```bash
python3 tools/param_table_check.py --list-rules
```

## 适配新 BMS 项目

1. 复制 `data/param_tables/example_bms_params.csv` 为项目专用文件，例如 `data/param_tables/project_x_params.csv`。
2. 按项目协议填充 `modbus_addr`，同一个地址只能归属一个参数；`u32/s32` 会占用 2 个连续 register。
3. 用 `c_name` 固定固件符号名，后续 C 代码、默认值表、Modbus 映射和文档都从该字段生成。
4. 将 `min`、`max`、`default` 统一定义为固件侧原始整数值，不直接填写浮点物理量。
5. 如果项目使用 Flash、EEPROM 或外部 NVM 持久化，将 `save_policy` 映射到项目现有保存流程；生成文件本身不直接写 Flash。
6. 每次修改参数源文件后，先运行检查脚本，再运行生成脚本，最后将生成文件纳入项目编译或发布包。

## 集成建议

- `generated/param_table.h` 可作为固件参数 ID、类型、默认值和元数据的统一入口。
- `generated/param_default.c` 提供 `g_param_table` 与 `g_param_defaults`，可接入上电参数初始化流程。
- `generated/modbus_param_map.c` 提供 Modbus 地址到参数 ID 的映射，可接入读写分发层。
- `docs/generated/param_table.md` 与 `docs/generated/modbus_register_map.md` 应随参数表同步提交，用于上位机、测试和生产交付确认。

## 维护规则

- 参数表源文件是第一可信来源。
- 自动生成文件带有“自动生成，请勿手动修改”说明，人工修改应回到 CSV/JSON。
- 修改客户可见寄存器地址、访问权限或单位时，必须同步做上位机和通信回归。
