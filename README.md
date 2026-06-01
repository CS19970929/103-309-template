# 103-309 BMS 项目

> STM32F103C8 + SH367309 电池管理系统固件
> 三元锂/磷酸铁锂 10串电池包保护板

---

## 快速链接

| 需求 | 入口 |
|------|------|
| 📖 **全部文档索引** | [docs/INDEX.md](docs/INDEX.md) |
| 📋 模块完整参考 | [docs/reference/module_reference.md](docs/reference/module_reference.md) |
| ⚙️ 宏配置速查 | [docs/reference/macro_config_reference.md](docs/reference/macro_config_reference.md) |
| 📊 全局变量清单 | [docs/reference/global_variables.md](docs/reference/global_variables.md) |
| 🗺️ 模块地图 | [docs/reference/module_map.md](docs/reference/module_map.md) |
| 🔌 Modbus 地址 | [docs/reference/COMMUNICATION_ADDRESS_INDEX.md](docs/reference/COMMUNICATION_ADDRESS_INDEX.md) |
| 📝 待办事项 | [TODO.md](TODO.md) |
| 🤖 AI 行为规范 | [AGENTS.md](AGENTS.md) |
| 🧹 量产清理分析 | [docs/reference/production_cleanup_analysis.md](docs/reference/production_cleanup_analysis.md) |

---

## 工程结构

```
├── 103 + 309/Project/         ← BMS App Keil 工程 (STM32F103C8)
│   └── Source/                ← 固件源码
├── firmware/comm_tool_f103ret6/ ← Comm Tool 固件 (STM32F103RET6)
├── tools/                     ← PC 上位机/工具脚本
├── docs/                      ← 全部文档
│   ├── INDEX.md               ← 文档总入口
│   ├── reference/             ← 核心参考
│   ├── design/                ← 设计文档
│   ├── protocol/              ← 协议文档
│   └── devlog/                ← 开发日志
├── upgrader_mcu/              ← 升级工具 MCU
├── build/                     ← 构建输出
└── dist/                      ← 发布输出
```

## 核心功能

- **保护**: AFE SH367309 硬件保护 + 三级软件保护参数
- **SOC**: 安时积分 + OCV 校准 + 静置补偿 + 显示平滑
- **通讯**: 串口 Modbus RTU + CAN 飞道协议
- **升级**: IAP Bootloader, 支持串口和 CAN 两种升级路径
- **低功耗**: RTC STOP HICCUP 模式, 定时唤醒 CAN 服务
- **显示**: 5-GPIO Charlieplexing 数码管
- **老化**: 工厂老化模式, CAN 远程控制

## 构建

- Keil MDK-ARM, STM32 标准外设库 V3.5.0
- 构建配置: `Project_Config.h` (Keil Configuration Wizard 可视化)
- Release: `PROJECT_CFG_BUILD_PROFILE=0`
