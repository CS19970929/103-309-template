# 103 + 309 应用层替换完成报告

本文记录 `codex/full-rewrite-2026-05-12` 分支在删除旧应用层后的替代工程状态。

## 1. 当前结论

旧 `103 + 309/Project/Source` 应用层 C/H 源码已经退役删除。当前有效应用工程是：

```text
firmware_rewrite/
```

新工程已经具备完整的 host 可测业务闭环：

- 统一 App 上下文：`bms_app_t`
- 固件主循环入口：`bms_firmware_run_once()`
- SOC 策略：积分、满电、低压、静置 OCV、sag holdoff、显示平滑
- 保护与 MOS 输出：OVP/UVP/OCP/温度/电压异常
- 通信兼容：`0x1005`、`0x2200`、`0x2318~0x231B`、`0xD000`、`0xFFFD`
- CAN/RTC 策略：有设备 `1s`、无设备 `10s`、探测恢复
- UI 策略：短按显示、长按深睡、`GPIO_MCU_WK` 持续显示
- SOC 双槽快照：版本、序号、CRC、读回校验
- 平台输出回调：MOS、显示、CAN、RTC、IAP
- STM32F1 SPL port 边界：`firmware_rewrite/ports/stm32f1_spl`
- Keil 工程文件已切到 rewrite 文件列表，两个 target 的 App 起始地址均为 `0x08004800`

当前还没有在 Windows/Keil 上实际编译，也没有做真实硬件接线和烧录验证，因此不能声明“已完成上板出货验证”。但是从仓库结构和工程引用上，旧应用层已经不再是实现来源。

## 2. 删除范围

已删除旧应用层约 `3.2 万行`，包括：

- 旧 `main.c`
- 旧 SOC / `SocEnhance`
- 旧 CAN / RTC / 低功耗
- 旧 LED / DI1
- 旧 EEPROM / Flash 应用层
- 旧 Fault / System_Monitor
- 旧 EasyLogger 接入
- 旧通信处理

保留内容：

- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0`：厂商 SPL / CMSIS / 启动文件
- `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`：已替换为 rewrite core + STM32F1 port 文件列表，不再引用旧 `..\Source`
- `tools/soc_flash_app_safe.ps1`：安全烧录脚本，继续保留 `0x08004800` App 地址检查
- 根目录历史文档：作为需求和验收清单

## 3. 新模块清单

| 模块 | 文件 | 状态 |
| --- | --- | --- |
| 公共 API | `include/bms_app.h` | 已完成 |
| 固件运行入口 | `include/bms_firmware.h`, `src/bms_firmware.c` | 已完成 |
| App 聚合 | `src/bms_app.c` | 已完成 |
| SOC | `src/bms_soc.c` | 已完成 host 版本 |
| 通信寄存器 | `src/bms_comm.c` | 已完成核心地址 |
| CAN/Power | `src/bms_power_can.c` | 已完成策略 |
| 保护/MOS | `src/bms_protection.c` | 已完成基础保护 |
| UI/IAP | `src/bms_ui_iap.c` | 已完成基础体验 |
| 存储 | `src/bms_storage.c` | 已完成内存双槽模型 |
| STM32F1 port | `ports/stm32f1_spl/*` | 已有 Keil 入口与弱符号硬件边界，待真实板级覆盖 |
| Keil 工程 | `103 + 309/Project/Users/*.uvprojx` | 已指向 rewrite 文件和 `0x08004800` App 起点，待 Windows/Keil 编译 |
| Host 测试 | `tests/test_rewrite_core.c` | 已完成核心行为测试 |

## 4. 已验证行为

`tools/run_rewrite_host_tests.py` 和 CMake/CTest 覆盖：

1. `0x1005` 单次设置 SOC。
2. `0xD000` offset `52..59` 输出 SOC、SOH、容量、循环和故障。
3. 满电电压锚定逐步到 `100%`。
4. 低压末端收敛到 `0%`。
5. 静置 OCV 只记录 deferred target，不立即跳变。
6. 久置低 OCV 单次只下修 `1%`。
7. 大电流 sag holdoff 阻断宽范围低压误校。
8. CAN 无 ACK 后切换 `10s` RTC，ACK 后恢复。
9. SOC 双槽快照保存与恢复。
10. UVP 保护关闭放电 MOS，并通过 `0xD000` 上报。
11. 短按显示、`GPIO_MCU_WK` 持续显示、长按深睡请求。
12. `0xFFFD` IAP 请求回调。
13. `bms_firmware_run_once()` 统一主循环入口。
14. STM32F1 SPL port 与 `bms_main_stm32f1_spl.c` 可编译。
15. Keil `.uvprojx` 不再引用旧 `..\Source` 应用层。

## 5. 验证命令

```bash
python3 tools/project_check.py --quiet
python3 tools/run_rewrite_host_tests.py
cmake -S firmware_rewrite -B build/firmware_rewrite_cmake
cmake --build build/firmware_rewrite_cmake
ctest --test-dir build/firmware_rewrite_cmake --output-on-failure
git diff --check
```

## 6. 上板前必须完成

这些属于硬件 port 工作，不应回到旧应用层写法：

1. 在 `ports/stm32f1_spl` 接入真实 AFE/ADC 采样，填充 `bms_sample_t`。
2. 接入真实 Flash 擦写，把 SOC 双槽映射到 `0x0801E000` / `0x0801E800`。
3. 接入 RS485 帧解析，调用 `bms_comm_write_single()` / `bms_comm_write_block()` / `bms_comm_read_d000()`。
4. 接入 CAN 发送，按照 core 的 pending 状态发送状态帧或探测帧。
5. 接入 RTC STOP，按 `bms_can_idle_rtc_period_seconds()` 给出的周期进入休眠。
6. 接入 LED/DI1/`GPIO_MCU_WK`。
7. 在 Windows/Keil 上打开现有 target 重新生成，确认输出 App 仍从 `0x08004800` 运行。
8. 使用 `tools/soc_flash_app_safe.ps1` 烧录 App，禁止裸写 `0x08000000`。

## 7. 工程规则

- 旧 `Source` 不再允许新增 C/H 应用源码。
- 新业务必须写入 `firmware_rewrite/src`。
- 硬件差异只写入 `firmware_rewrite/ports/*`。
- 检查脚本 `tools/project_check.py` 已经把旧 `Source` 中存在 C/H 文件视为失败。
