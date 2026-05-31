# 编译优化迁移检查

记录日期：2026-05-31

## Keil 优化配置

| 配置项 | Keil 值 | GCC/CMake 目标 | 风险 |
|---|---|---|---|
| Release 全局优化 | `Optim=1`，`oTime=0` | `-Os -g` | 中 |
| Debug 全局优化 | `Optim=1`，`oTime=0` | `-Og -g3` | 中 |
| `LedBar.c` 文件级优化 | `Optim=0`，`oTime=2` | 单文件 `-O0` | 中 |
| `-O3` | 未使用 | 不使用 | 低 |

## 时序敏感模块风险

| 模块 | 风险 | 当前处理 |
|---|---|---|
| `LedBar.c` | GPIO/扫描时序敏感，Keil 已单文件 O0 | GCC 保留单文件 `-O0` |
| `I2C_AFE1.c` / `SH367309_Func.c` | 软件 I2C/AFE 时序风险 | 先不强制 O0，构建后用硬件验证；必要时再单文件降优化 |
| `System_Init.c` | `__delay_ms/us` 与 10ms tick | 需硬件验证 |
| `Can_HDX.c` | CAN 发送调度和低功耗唤醒 tick | 需硬件验证 |
| `SocEnhance.c` / `SOC.c` | SOC 定时积分和持久化保存 | 需对照运行日志 |
| `rtc_sleep.c` / `LowPowerSleep.c` | STOP/RTC 唤醒时序 | 需硬件验证 |

## 策略

- Release 先用 `-Os`，更接近嵌入式固件尺寸目标。
- Debug 用 `-Og -g3`，优先保证 VS Code/GDB 调试体验。
- 不为了编译通过删除业务代码。
- 单文件降优化必须有 Keil 基准或实际硬件证据；当前仅 `LedBar.c` 已有 Keil 文件级 O0 证据。
