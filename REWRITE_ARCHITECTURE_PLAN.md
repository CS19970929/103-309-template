# 103 + 309 程序重写架构总纲

本文用于固定重写边界，避免依赖单次对话记忆。后续重写代码、烧录脚本、测试脚本都必须遵守本文的地址、配置、协议和验证规则。

## 目标

- 架构简单：主循环、采样、SOC、保护、控制、低功耗、通信、存储各自职责清晰。
- 行为稳定：MOS 控制、低功耗进入、SOC 保存、参数写入都只有一个入口和一个提交点。
- 兼容现有上位机：保留当前 RS485/CAN 关键寄存器和测试入口的兼容语义。
- 量产隔离：量产程序默认关闭 SOC 注入式测试入口，测试配置不能影响出货固件。
- 烧录安全：App 永远从 `0x08004800` 运行和升级，不能覆盖 IAP。

## 绝对安全边界

| 项目 | 固定值 / 要求 |
| --- | --- |
| IAP 起始地址 | `0x08000000` |
| App 起始地址 | `0x08004800` |
| App 安全结束地址 | `0x0801BFFF` |
| 参数区起始地址 | `0x0801C000` |
| App scatter 文件 | `103 + 309/Project/Users/Objects/FD_Release.sct` |
| 安全烧录脚本 | `.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash` |

禁止把 `FD_Debug.bin` 或 `FD_Release.bin` 裸写到 `0x08000000`。修改或新增烧录脚本时，必须保留 `0x08004800` 地址检查和 dry-run 输出。

## Flash 分区

当前重写按 128KB Flash 布局处理。若实物 MCU 只有 64KB，必须重新设计 App/IAP/参数存储，不能直接沿用当前分区。

| 区域 | 地址 |
| --- | --- |
| IAP/Bootloader | `0x08000000..0x080047FF` |
| App | `0x08004800..0x0801BFFF` |
| AFE 参数 A/B | `0x0801C000` / `0x0801C800` |
| 运行参数 A/B | `0x0801C400` / `0x0801CC00` |
| 日志 A/B | `0x0801D000` / `0x0801D800` |
| SOC 快照 A/B | `0x0801E000` / `0x0801E800` |
| 升级策略 | `0x0801F000` |
| IAP 更新标志 | `0x0801F800` |
| 睡眠标志 | `0x0801FC00` |

## 编译配置隔离

| 场景 | 配置 |
| --- | --- |
| 量产 | `PROJECT_CFG_BUILD_PROFILE 0` |
| 调试 | `PROJECT_CFG_BUILD_PROFILE 1` |
| SOC 测试固件 | `PROJECT_CFG_BUILD_PROFILE 2` + `PROJECT_CFG_SOC_TEST_MODE_ENABLE 1` + `PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX 300` |

量产固件读取 `0xD300 supported=0` 是正常结果，代表 MCU 注入式 SOC 测试入口已关闭。SOC 测试结束后必须恢复量产配置，并通过 `COM4/19200/slave=1` 读取 `0xD000` 和 `0xD300` 确认状态。

## 新架构分层

```text
platform/      时钟、GPIO、RTC、Flash、USART、CAN、IWDG
drivers/       SH367309、LED、按键、MOS 低层驱动
model/         BmsConfig、BmsSample、BmsState、BmsSnapshot
core/protect/  保护判断，只输出 fault/allow
core/control/  MOS、加热、均衡的唯一策略出口
core/soc/      简化 SOC 状态机
core/power/    单一低功耗状态机
protocol/      RS485/CAN 寄存器兼容层
storage/       参数、日志、SOC 快照双槽
app/           app_init/app_run 调度
```

驱动层只负责硬件读写，不直接修改策略结果。保护层只产生故障和允许位，不直接操作 MOS。控制层是唯一允许提交 MOS、加热、均衡硬件动作的模块。

## 主循环任务

重写后继续使用裸机超循环，不引入 RTOS。

| 周期 | 任务 |
| --- | --- |
| 每轮 | 通信接收、发送泵、看门狗喂狗条件检查 |
| 10ms | 按键/LED 扫描、轻量状态机推进 |
| 100ms | 保护判断、控制策略、通信状态刷新 |
| 200ms | AFE 采样、SOC 积分、温度/电流处理 |
| 1000ms | 低功耗仲裁、参数延迟保存、日志节流、运行计数 |

所有周期 flag 由系统 tick 统一产生，业务模块不能自己创建第二套全局时基。

## SOC 重写范围

第一版 SOC 只保留稳定且可解释的功能：

- 启动：优先读取 SOC 快照，快照无效时用 OCV 表估算。
- 运行：200ms 根据净电流做安时积分。
- 端点：满电确认校准到 100%，欠压端点校准到低 SOC。
- 保存：周期保存、入睡前保存、关键端点保存。
- 显示：`display_soc` 独立平滑，不反向污染真实剩余容量。

第一版不做 EKF、复杂动态阻抗模型、过多中低压 holdoff 表。后续若要增强，只能在 `core/soc` 内部扩展，不能把 SOC 逻辑散到通信、LED、低功耗或 AFE 驱动中。

## 低功耗重写范围

低功耗只保留一套状态机：

```text
POWER_RUN -> POWER_RTC_IDLE -> POWER_STOP -> POWER_RUN
          -> POWER_DEEP_PENDING -> POWER_STOP
```

唯一请求入口：`Power_Request(reason, mode)`。

唯一仲裁入口：`Power_Evaluate(snapshot, config)`。

唯一提交入口：`Power_CommitSleep()`。

入睡提交必须按固定顺序执行：停止新通信发送、`Can_PrepareSleep()`、SOC 快照、LED 睡眠显示保存、AFE sleep、RTC/EXTI 配置、进入 STOP。唤醒后只由 `Power_HandleWakeup()` 记录唤醒源和 RTC 休眠秒数。

重写后不再同时维护 `g_sleepModeSelect` 和 `Sleep_Mode` 这类双状态。

## 通信兼容边界

RS485 继续保留 Modbus 类 `0x03`、`0x06`、`0x10`。以下地址优先保持兼容：

| 地址 | 用途 |
| --- | --- |
| `0xD000` | 板端运行状态 |
| `0xD300` | SOC 测试状态 |
| `0x1005` | 设置一次 SOC |
| `0x2200` | SOC OCV/相关表参数 |
| `0x2318..0x231B` | 容量、循环、V100、V0 |
| `0x2400` | AFE 参数 |

协议层只能做寄存器解析和数据映射，不能直接写 Flash、不能直接控制 MOS、不能直接进入低功耗。

## 验证门禁

每次较大迁移至少执行以下检查，无法执行时必须在提交说明或测试记录中写明原因：

1. `tools/project_check.py`
2. `tools/run_soc_host_c_test.py`
3. `tools/soc_replay_test.py`
4. Keil Debug/Release 编译
5. 安全脚本 dry-run 确认 App 起始 `0x08004800`
6. 上板通过 `COM4/19200/slave=1` 读 `0xD000` 和 `0xD300`
7. 低功耗 STOP/RTC 唤醒实测
8. CAN 250kbit/s 周期发送和入睡前发送停止验证

## 迁移顺序

1. 固定 Flash/IAP/App 边界，修正 scatter。
2. 建立 `103 + 309/Project/Rebuild` 骨架，不接入 Keil 工程。
3. 移植平台层和 AFE 采样，先能读出 `BmsSample`。
4. 移植通信只读状态，先打通 `0xD000`。
5. 移植存储、参数和 SOC 快照。
6. 实现简化 SOC，并跑主机测试。
7. 实现保护和唯一控制出口。
8. 实现低功耗状态机。
9. 接入 LED、CAN、日志。
10. 最后替换旧主循环并删除旧状态机。
