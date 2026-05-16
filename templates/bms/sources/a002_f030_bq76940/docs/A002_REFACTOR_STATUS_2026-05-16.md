# A002 F030 + BQ76940 Port 参考源状态

## 当前原则

- A002 旧项目已作为 `a002_f030_bq76940` port 参考源纳入当前仓库。
- 通用模板应用层不使用旧 A002 逻辑；A002 只提供 F0 MCU 驱动、BQ76940 AFE 驱动、Keil/F0 地址和硬件资源参考。
- 外部 EEPROM 完全废除，后续不再使用 24xx EEPROM、PB3/PB4 软件 I2C 或 PA15 WP。
- 参数、日志、SOC、生产信息统一使用 MCU 内部 Flash 存储区 `0x0800E000..0x0800FFFF`。
- 当前参考源使用 `Storage_*` API；旧 `EEPROM` / `E2P` 符号只作为历史逻辑地址和兼容包装保留。
- 当前低功耗源码只有 `SleepDeal` + `RTC` 路径，没有 `IdleSleep` 源码。

## 已完成

| 项目 | 状态 |
| --- | --- |
| Flash/IAP 地址 | App 固定 `0x08001C00..0x0800DFFF`，Storage 固定 `0x0800E000..0x0800FFFF`。 |
| Keil scatter | `Objects/CommomBQ769x0_16series_030C8T6_C.sct` 与 profile 地址一致。 |
| 外部 EEPROM | 24xx 驱动、旧 `param` 草稿和 WP/I2C 路径已从模板源删除。 |
| 内部 Flash 写入 | `FlashWriteOneHalfWord()` 使用整页缓存、擦除、恢复，避免单 halfword 写入破坏同页其他参数。 |
| 存储入口 | `main.c` 改为 `Storage_Init()` / `Storage_Task()`，旧 `InitE2PROM()` / `App_E2promDeal()` 仅兼容。 |
| 功能门控 | `main.c` 已按 `PROJECT_FEATURE_SOC/LOW_POWER/RTC/RS485/HEAT/LEDBAR` 调度。 |
| 软件保护 | A002 profile 固定为 MCU 软件保护主线，`Fault.c` 受 `PROJECT_FEATURE_SOFTWARE_PROTECTION` 门控。 |
| IdleSleep | 文档已改为废止说明，不再把不存在的 `IdleSleep` 当作当前路径。 |
| SOC 快照边界 | 修正启动读取 SOC/放电累计环形槽边界，避免把相邻字段误读为有效槽。 |

## 后续只保留的参考价值

1. MCU port：startup、system clock、interrupt、Flash/IAP、RTC、GPIO/EXTI、watchdog、Keil/scatter。
2. AFE port：BQ76940 I2C、寄存器访问、采样数据、MOS 控制、SCD/OCD latch 状态。
3. 地址 profile：F0 IAP/App/Storage 地址、内部 Flash page size、scatter 写法。
4. 硬件资源：旧板 GPIO 占用和冲突关系，作为生成器资源检查参考。
5. 不再继续改造旧 A002 SOC、低功耗、软件保护和通信应用流程；这些统一由当前项目应用层模板承载。

## 验收规则

- `python3 tools/project_check.py --quiet` 必须通过。
- A002 模板源不得出现已删除的外部 EEPROM 驱动、Keil 构建产物或旧 `old *` 文件。
- 后续新功能必须优先接入 `Project_Template_Config.h` 和 `Project_Features.h`，不要重新散落条件宏。
