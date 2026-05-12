# 103 + 309 从零重写需求与架构说明

本文是 `codex/full-rewrite-2026-05-12` 分支的重写输入与落地说明。目标不是复刻旧应用层实现，而是从仓库文档中抽取必须保留的产品行为、通信契约、安全边界和用户体验要求，再用更简单的裸机服务化架构重新实现。

## 1. 文档读取范围

本轮已扫描仓库内 Markdown / txt / rst / adoc 文档，排除外部 F0 demo、Keil DebugConfig 和 `.git`。扫描到 `70` 个项目文档，总计约 `14946` 行。

重点深读文档包括：

- `README.md`
- `运行架构与时基重构方案.md`
- `SOC_MODULE_LOGIC.md`
- `SOC_CALIBRATION_STRATEGY.md`
- `SOC完整运行流程说明.md`
- `COMMUNICATION_ADDRESS_INDEX.md`
- `COMMUNICATION_LAYOUT_REPORT.md`
- `COMMUNICATION_WRITE_DETAIL.md`
- `STORAGE_LAYOUT_REPORT.md`
- `MCU资源分布与架构优化评估.md`
- `休眠低功耗逻辑梳理与优化建议.md`
- `RTC_CAN自适应休眠说明.md`
- `LED软件框架与时序梳理.md`
- `SOC_MCU_SAFE_FLASH_GUIDE.md`
- `SOC_TEST_REQUIREMENTS_SUMMARY.md`
- `项目自动化检查与发布流程.md`

本轮不以旧 `main.c` / `SocEnhance.c` / `Can_HDX.c` 等应用层写法作为架构依据，只把文档中已经沉淀的外部行为当作需求。

## 2. 重写总目标

1. 架构简单：单一 `BmsApp` 上下文，运行调度、SOC、通信、低功耗、CAN、存储各自内聚，模块之间只通过明确数据结构交互。
2. 状态机清晰：每个业务只有一条主路径，不再保留多套休眠模式仲裁、多个 tick 所有者、多个 CAN 时间源。
3. 运行稳定：所有外部写入有范围校验，Flash 快照有双槽、版本、序号和 CRC，CAN 无 ACK 不阻塞 RTC。
4. 用户体验好：SOC 能到 `100%`、能到 `0%`，运行态自动校准每次最多 `1%`，静置 OCV 不突然跳变，低压末端更快收敛。
5. 量产安全：IAP/App 地址、SOC 测试模式、烧录脚本和上位机启动方式必须固定在文档或脚本中。
6. 可验证：先提供 host C 测试，后续硬件 port 接入后再补 Keil/上板验证。

## 3. 必须保留的外部契约

### 3.1 烧录与构建

- IAP/Bootloader 起始地址：`0x08000000`。
- App 起始地址：`0x08004800`。
- 禁止将 `FD_Debug.bin` / `FD_Release.bin` 裸写到 `0x08000000`。
- App 烧录必须保留安全脚本入口：`tools/soc_flash_app_safe.ps1`，且继续检查 `0x08004800`。
- 量产配置必须保持 `PROJECT_CFG_BUILD_PROFILE 0`。
- SOC 测试固件才允许 `PROJECT_CFG_BUILD_PROFILE 2`、`PROJECT_CFG_SOC_TEST_MODE_ENABLE 1`。
- 上位机固定从 `tools/start_soc_test_ui.ps1` 启动，不直接双击 Python 文件。

### 3.2 通信地址

- `0x03 / 0xD000`：状态区必须保持字段顺序，SOC 在 offset `52`，SOH 在 `53`，容量与循环在 `54..57`。
- `0x06 / 0x1005`：单次设置 SOC，内部 SOC 和显示 SOC 立即同步，并触发快照保存。
- `0x10 / 0x2200~0x2229`：21 组 `(mV, SOC%)` OCV 表，当前重写保持 RAM 生效。
- `0x10 / 0x2318~0x231B`：SOC 基础参数，容量、循环次数、V100、V0。
- 非法地址、非法长度、非法 SOC 值必须拒绝，不做隐式修正。

### 3.3 SOC 用户体验

- 主线为安时积分，电压只在可信场景作为锚点或上限。
- 自动校准每次最多 `1%`，适用于满电、低压、中低压、静置/RTC OCV。
- 充电不能单靠积分直接显示 `100%`，必须满足满电电压确认后逐步到 `100%`。
- 低压末端必须能到 `0%`，`3000mV` 以下要加快显示收敛，避免实际保护时 SOC 仍很高。
- 大电流压降和刚松油门未回弹时不能随意按低电压校准。
- 静置 OCV 先记录 `deferred target`，不在停车静置时立即追平；久置低 OCV 允许慢速下修。
- SOH 采用稳定可解释口径：按循环次数映射，默认下限 `80%`，不做复杂在线容量学习。

### 3.4 低功耗 / RTC / CAN

- 运行期只有一个 Power Manager 负责能否休眠，不让 CAN、LED、SOC 各自抢低功耗判定权。
- RTC 唤醒后必须完成必要的 SOC 静置补偿和 CAN 服务窗口，再决定继续睡或退出。
- CAN 总线上有设备时，RTC 周期为 `1s`，唤醒后保持周期业务帧。
- CAN 总线上无设备时，RTC 周期为 `10s`，只发送轻量探测帧，不补发完整业务帧。
- 连续无 ACK 后切到无设备策略；任意 ACK 或 RX 立即恢复有设备策略。
- CAN 发送失败、无 ACK、BusOff、对端不在线不能让系统长期进不了 RTC。

### 3.5 LED / 开关体验

- 正常静置不显示，短按 DI1 立即显示当前 SOC，松开后约 `5s` 熄灭。
- 长按 DI1 约 `3s` 进入深度休眠。
- 休眠中短按显示休眠前 SOC，长按进入正常开机流程。
- `GPIO_MCU_WK` 有效时持续显示 SOC，并阻断自动 RTC/低压休眠累计。
- 进入 STOP 前 LED GPIO 必须进入确定关断态，避免段位误亮。

### 3.6 存储与升级

- SOC 快照使用双槽 journal：版本、序号、CRC、有效性校验。
- 参数、日志、SOC、升级标志必须和 App/IAP 区隔离。
- `0x0801C000~0x0801FFFF` 后 64K 参数区只在实际 Flash >= 128KB 时可靠，后续硬件 port 必须读取 Flash 容量并产测验证。
- 写参数、写 AFE、写 Flash 后要能读回校验；重写架构中统一由 storage/parameter service 承担。

## 4. 新实现目录

```text
firmware_rewrite/
  include/bms_app.h
  src/bms_app.c
  src/bms_comm.c
  src/bms_power_can.c
  src/bms_soc.c
  src/bms_storage.c
  tests/test_rewrite_core.c
tools/run_rewrite_host_tests.py
```

当前实现是 clean-room host-portable core，不包含旧应用层源码，不包含 STM32 SPL 外设驱动拷贝。后续接板时只需要新增 `ports/stm32f1_spl/`，把 AFE、ADC、CAN、SCI、RTC、Flash、GPIO 接到当前 core API。

## 5. 模块边界

| 模块 | 文件 | 职责 |
| --- | --- | --- |
| App | `bms_app.c` | 单一上下文初始化、采样推进、RTC 唤醒入口、快照保存 |
| SOC | `bms_soc.c` | 积分、满电、低压表、中低压、sag holdoff、静置 OCV、显示平滑 |
| Comm | `bms_comm.c` | `0x1005`、`0x2200`、`0x2318~0x231B`、`0xD000` 兼容层 |
| Power/CAN | `bms_power_can.c` | RTC 进入判定、深睡判定、CAN 有/无设备策略 |
| Storage | `bms_storage.c` | SOC 双槽快照、版本、序号、CRC、读回验证 |
| Host Test | `tests/test_rewrite_core.c` | 需求级行为验证 |

## 6. 当前已覆盖的行为

| 行为 | 覆盖状态 |
| --- | --- |
| `0x1005` 单次设置 SOC | 已实现并测试 |
| `0xD000` SOC/SOH/容量/循环输出 | 已实现并测试 |
| 满电逐步到 `100%` | 已实现并测试 |
| 低压末端到 `0%` | 已实现并测试 |
| 静置 OCV deferred target 不跳变 | 已实现并测试 |
| 久置低 OCV 每次最多下修 `1%` | 已实现并测试 |
| 大电流 sag holdoff 阻断宽范围低压校准 | 已实现并测试 |
| CAN 有设备 `1s`、无设备 `10s` | 已实现并测试 |
| 无设备 RTC 探测帧、ACK 恢复 | 已实现并测试 |
| SOC 双槽快照恢复 | 已实现并测试 |
| STM32 SPL 硬件 port | 尚未接入，本分支先完成核心架构 |
| Keil 工程集成 | 尚未接入，需后续新增独立 target |
| AFE/保护参数真实写入 | 尚未接入，需硬件 port 和参数 service |
| LED Charlieplexing 真实扫描 | 尚未接入，当前先保留体验契约 |

## 7. 验证入口

当前可在 macOS / Linux / Windows Git Bash 下运行：

```bash
python3 tools/run_rewrite_host_tests.py
```

测试覆盖：

1. `0x1005` 设置 SOC 和 `0xD000` 输出一致性。
2. 高压满电确认逐步到 `100%`。
3. 低压表快速收敛到 `0%`。
4. 静置 OCV 只记录 target，不立即跳变。
5. 久置低 OCV 单次只下修 `1%`。
6. 大电流 sag holdoff 阻断宽范围低压校准。
7. CAN 无 ACK 切 `10s` RTC，有 ACK 恢复 `1s`。
8. SOC 快照双槽保存和恢复。

## 8. 后续接板顺序

1. 新增 `ports/stm32f1_spl/`，只写硬件适配，不改 core 业务逻辑。
2. 新增独立 Keil target，例如 `FD_Rewrite_Debug` / `FD_Rewrite_Release`，不要直接替换旧 target。
3. 接入 AFE/ADC 采样，将真实 `200ms` 样本送入 `bms_app_process_sample()`。
4. 接入 RS485/CAN，将协议解析结果调用 `bms_comm_*()`。
5. 接入内部 Flash 双槽，把 `bms_storage_t` 映射到真实 Flash page，并做擦写读回验证。
6. 接入 RTC Stop，唤醒后调用 `bms_app_apply_rtc_wake()`。
7. 接入 LED/DI1，按短按 `5s`、长按 `3s`、`GPIO_MCU_WK` 持续显示验收。
8. 最后才迁移 IAP/安全烧录链路，继续使用 `0x08004800` App 地址检查。

## 9. 架构取舍

- 不引入 RTOS。当前需求用单一 runtime + service 状态机足够，RTOS 会增加资源和调试复杂度。
- 不做复杂 FCC 在线学习。SOC 用户体验优先，先用可解释 SOH 映射和稳定端点校准。
- 不做多套兜底状态机。异常只做明确拒绝、保持原状态或请求安全休眠。
- 不把硬件驱动写进 core。核心可 host 测试，硬件差异留给 port 层。
