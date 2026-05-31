# 103-309 BMS 项目文档入口

> 源码是第一可信来源。文档与源码冲突时以源码为准。

## 快速入口

| 用途 | 文档 |
|------|------|
| **项目概览** | [docs/README.md](docs/README.md) — 权威文档总入口 |
| **协作规则** | [AGENTS.md](AGENTS.md) — 仓库协作、烧录安全、SOC 测试隔离 |
| **项目简介** | [CLAUDE.md](CLAUDE.md) — 项目性质、目录结构、使用约定 |
| **待办事项** | [TODO.md](TODO.md) — 当前待办任务 |
| **架构** | [docs/architecture.md](docs/architecture.md) — 软件分层、主循环、任务调度 |
| **模块地图** | [docs/module_map.md](docs/module_map.md) — 源码文件与模块对应关系 |
| **设计文档** | [docs/design/](docs/design/) — 各模块设计说明 |
| **通信协议** | [docs/protocol/](docs/protocol/) — Modbus/CAN/UART 协议说明 |
| **测试计划** | [docs/test/test_plan.md](docs/test/test_plan.md) — 全项目测试计划 |

## 高风险修改前必读

修改烧录地址、APP/IAP、AFE 参数、协议写入口、低功耗唤醒、存储策略前，请先阅读 `docs/review/` 下对应的门禁方案和用户确认包。

## 文档结构

```
docs/
├── README.md                  ← 文档入口（起点）
├── project_overview.md        ← 项目总览
├── architecture.md            ← 软件架构
├── module_map.md              ← 模块与文件映射
├── design/                    ← 各模块设计
├── protocol/                  ← 通信协议
├── test/                      ← 测试计划
├── workflow/                  ← 开发工作流
├── review/                    ← 审查与门禁方案
├── changelog/                 ← 变更记录
├── generated/                 ← 自动生成文档
└── archive/                   ← 归档旧文档（不作为当前参考）
```
