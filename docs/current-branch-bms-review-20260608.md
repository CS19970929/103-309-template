# 当前分支 BMS 项目 Review 报告（2026-06-08）

## 1. Review 范围

- 工作目录：`/Users/cs/Downloads/103-309-template`
- 当前分支：`codex/d009-can-host-comm`
- 当前 HEAD：`33d15db Harden BMS IAP upgrade guards`
- Review 类型：只读源码审查，未修改业务代码
- 重点范围：
  - BMS App 主循环、AFE 采样、电流换算、保护与休眠链路
  - CAN service / IAP 升级链路
  - comm tool / host 工具与 BMS App 协议一致性
  - Keil 工程、Flash/IAP 分区与文档配置一致性

当前 App 主循环入口在 `103 + 309/Project/Source/main.c`：

```c
App_SysTime();
App_AFEGet();
App_WarnCtrl();
App_Sci();
App_E2promDeal();
App_CellBalance();
App_Can();
App_SleepDeal();
App_SOC();
App_FlashUpdate();
```

## 2. 结论摘要

当前分支已经形成了 D009 CAN host communication + BMS CAN IAP 的基本链路，IAP 升级保护和 host 侧 guard 测试可运行。但从发布风险看，仍有 3 个需要优先处理的问题：

1. MCU/Flash 型号口径与实际分区不一致，当前配置依赖 128KB Flash，但工程目标仍写 `STM32F103C8`。
2. 电流校准存在 signed/unsigned 混算，负 B 校准值可能包装成巨大正电流。
3. AFE 采样被 SCI/EEPROM 忙状态跳过时，没有进入 stale/fail 保护链，可能长期使用旧采样数据。

另外，comm tool 暴露的 BMS aging 命令与 BMS App 当前命令集不匹配；CAN 写参数和 IAP 入口也需要按产品暴露面确认授权边界。

## 3. 主要问题

### P1：MCU/Flash 口径冲突，IAP 分区依赖 128KB Flash

证据：

- App Keil 工程目标是 `STM32F103C8`，但 IROM 配置为 `0x08004800 + 0x1B000`，实际使用到 `0x0801F800` 前。
  - `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx:17`
  - `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx:21`
  - `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx:24`
- IAP 工程同样标为 `STM32F103C8`，但使用 `STM32F10x_128.FLM`。
  - `firmware/bms_iap_f103c8/keil/BMS_CAN_IAP_F103C8.uvprojx:17`
  - `firmware/bms_iap_f103c8/keil/BMS_CAN_IAP_F103C8.uvprojx:21`
  - `firmware/bms_iap_f103c8/keil/BMS_CAN_IAP_F103C8.uvprojx:24`
- IAP 配置明确声明 128KB Flash：
  - `firmware/bms_iap_f103c8/source/iap/bms_iap_config.h:16`
  - `firmware/bms_iap_f103c8/source/iap/bms_iap_config.h:24`
- App Flash 地址也使用后 64KB 空间：
  - `103 + 309/Project/Source/Flash.h:9`
  - `103 + 309/Project/Source/Flash.h:12`
  - `103 + 309/Project/Source/Flash.h:13`

风险：

- 如果实物是标准 STM32F103C8 64KB，当前 App/IAP/flag page 布局不可发布。
- 如果实物是 128KB 兼容料或实际采购型号不是 C8，工程命名、Keil Device、文档和烧录脚本仍会误导生产、维修和后续固件升级。

建议：

1. 先用实物 BOM、芯片丝印、读 DBGMCU IDCODE/Flash size register 确认真实容量。
2. 若确认是 128KB 器件，把 App/IAP Keil Device、工程目录命名、README、升级文档和 host 工具中的容量口径统一。
3. 若必须兼容标准 C8，重新切分 IAP/App/flag page，不能继续使用 `0x0801F800` 和 `0x0801FC00`。

### P1：电流校准 signed/unsigned 混算可能导致负偏移包装

证据：

- `DataLoad_Current()` 使用 `UINT32 u32_ChgCur_mA`、`UINT32 u32_DsgCur_mA` 保存电流。
  - `103 + 309/Project/Source/DataDeal.c:364`
- 校准时把 signed B 值加入 unsigned 表达式。
  - `103 + 309/Project/Source/DataDeal.c:392`
  - `103 + 309/Project/Source/DataDeal.c:401`
- 后续 clamp 仍在 unsigned 变量上执行，`> 0` 无法拦住负数包装后的大正数。
  - `103 + 309/Project/Source/DataDeal.c:410`
  - `103 + 309/Project/Source/DataDeal.c:414`
- 上位机写入校准 B 值时允许负数，且写入 `INT16 g_i16CalibCoefB[]`。
  - `103 + 309/Project/Source/Sci_Upper.c:2590`
  - `103 + 309/Project/Source/Sci_Upper.c:2602`
  - `103 + 309/Project/Source/Sci_Upper.c:2627`

风险：

- 负 B 校准值可能让小电流或中等电流被包装成异常大电流。
- 影响范围包括电流上报、SOC 积分、均衡允许条件、休眠条件和部分保护逻辑。

建议：

1. 用 `INT32` 或 `INT64` 临时变量计算 `raw_mA * K + B * 1000`。
2. 在 signed 域内先 clamp 到 `0`，再转换为 `UINT32`。
3. 充电、放电两条路径可以收成一个很小的本地 helper，避免复制 signed/unsigned 细节。

### P1：AFE 采样跳过没有 stale/fail 保护

证据：

- `App_AFEGet()` 在 SCI 正在发送时直接返回。
  - `103 + 309/Project/Source/DataDeal.c:771`
- EEPROM 写标志存在时也直接返回。
  - `103 + 309/Project/Source/DataDeal.c:776`
- 只有 `UpdateVoltageFromBqMaximo()` 执行后返回失败，才会进入 `AFE_UpdateFailDeal()`。
  - `103 + 309/Project/Source/DataDeal.c:781`
  - `103 + 309/Project/Source/DataDeal.c:783`
- `AFE_UpdateFailDeal()` 内才会累计失败、清 stale runtime data 和触发 deep sleep。
  - `103 + 309/Project/Source/DataDeal.c:333`
  - `103 + 309/Project/Source/DataDeal.c:346`
  - `103 + 309/Project/Source/DataDeal.c:351`

风险：

- 如果 SCI TX、批量参数写入或某个 EEPROM 写标志长时间保持，AFE 数据不会刷新。
- 主循环后续 `App_WarnCtrl()`、`App_CellBalance()`、`App_SleepDeal()`、`App_SOC()` 会继续消费旧电压、旧电流、旧温度。
- 这类 stale 数据不会触发当前 AFE failure 计数，属于隐藏风险。

建议：

1. 为“跳过采样”增加独立 age 计数，超过阈值后进入现有 stale/fail 处理。
2. 区分短暂 defer 和异常 defer：短暂 defer 不报错，长时间 defer 必须置错误或清运行态。
3. EEPROM 写路径建议控制单次阻塞窗口，避免参数批量写入长时间挡住 AFE 采样。

### P2：comm tool 暴露 BMS aging 命令，但 BMS App 未实现

证据：

- comm gateway 定义并发送 aging 命令 `0x07..0x0A`。
  - `firmware/comm_tool_f103ret6/source/app/ct_can_gateway.h:15`
  - `firmware/comm_tool_f103ret6/source/app/ct_can_gateway.c:394`
- host CLI 暴露 `bms-aging`、`bms-aging-status`、`bms-aging-set-hours`。
  - `tools/comm_tool_host.py:39`
  - `tools/comm_tool_host.py:464`
  - `tools/comm_tool_host.py:724`
- BMS App 当前 CAN app command handler 只处理 `GET_STATUS`、`ENTER_IAP`、`READ_REG`、`READ_BLOCK`、`WRITE_PREP`、`WRITE_COMMIT`，默认返回 `BAD_CMD`。
  - `103 + 309/Project/Source/Can_HDX.c:1039`
  - `103 + 309/Project/Source/Can_HDX.c:1119`

风险：

- 上位机和产测人员会认为 aging 功能已支持，但设备实际不会执行。
- 如果 UI 没有明确 unsupported，会造成误判、返工或现场调试时间浪费。

建议：

1. 如果 D009 需要 aging 模式：在 BMS App 实现 `0x07..0x0A`，并定义状态、剩余时间、掉电恢复和安全退出策略。
2. 如果当前版本不需要 aging：先从 host/UI/comm gateway 中隐藏或禁用这些入口，并返回明确的 unsupported。

### P2：CAN 写参数/IAP 入口缺少明确产品级授权边界

证据：

- CAN app command 可通过 `ENTER_IAP` 请求进入 IAP。
  - `103 + 309/Project/Source/Can_HDX.c:1048`
- CAN app command 可读寄存器、写寄存器，写入最终走 `Sci_HostWriteWords()`。
  - `103 + 309/Project/Source/Can_HDX.c:1066`
  - `103 + 309/Project/Source/Can_HDX.c:1097`
  - `103 + 309/Project/Source/Can_HDX.c:1105`
- `Sci_HostWriteWords()` 主要做空指针、数量、范围和 busy 校验，没有区分 CAN service 的产品级授权状态。
  - `103 + 309/Project/Source/Sci_Upper.c:967`
  - `103 + 309/Project/Source/Sci_Upper.c:984`

风险：

- 如果该 CAN service 只用于受控产测工具，风险可接受。
- 如果 CAN 总线会暴露给整车、用户或第三方设备，参数写入和 IAP 入口需要额外授权。

建议：

1. 增加 session unlock、生产模式开关或编译开关。
2. 对 CAN 写参数做 allowlist，而不是直接复用所有 RS485 可写地址。
3. IAP 入口至少绑定设备地址、短时会话和明确的工具侧确认。

## 4. 其他观察

### 均衡链路

本轮未发现新的高风险问题。当前 `Cell_balance.c` 逻辑较保守：

- 1s 调用一次 `App_CellBalance()`。
- `CB_IsBalanceAllowed()` 包含 feature enable、cell count、电压范围、三级故障、AFE/SPI/CBC 错误、电流阈值、开启电压和压差窗口。
- `CB_BuildTargetMask()` 使用 3-cycle open filter 和 close-window hysteresis。
- AFE 写 `BALANCEH/M/L` 有 3 次重试。

后续更适合做实测验证：充电/放电电流边界、压差开关窗口、AFE 写失败、sleep 前关闭均衡。

### 低功耗链路

当前 `conf.h` 中 `__FUNC_RTC__` 未开启，主路径不是 runtime RTC sleep，而是 `SleepDeal.c` 写 `FLASH_ADDR_SLEEP_FLAG` 后走 reset/STOP 恢复链：

- `103 + 309/Project/Source/conf/conf.h:14`
- `103 + 309/Project/Source/SleepDeal.c:97`
- `103 + 309/Project/Source/SleepDeal.c:698`

如果后续重新启用 `rtc_sleep.c`，需要重新核对 CAN、AFE、ADC、IO、IWDG、LED 的睡前和唤醒恢复闭环。

## 5. 验证结果

已通过：

```bash
python3 tools/iap_upgrade_guard_test.py
python3 -m py_compile tools/comm_tool_host.py tools/comm_tool_upgrade_ui.py tools/iap_upgrade_guard_test.py
git diff --check
```

未执行：

- Keil / ARMCC 固件实际编译：当前 macOS 环境未提供 Keil 编译链。
- 硬件实测：需要真实 D009/BMS 板、CAN 工具和升级流程验证。
- `tools/project_check.py`、`tools/soc_replay_test.py`：当前 checkout 不存在这两个脚本，不能作为本轮门禁。

## 6. 建议处理顺序

1. 确认真实 MCU/Flash 容量，并统一 App/IAP/host/doc 的容量和型号口径。
2. 修复 `DataLoad_Current()` signed/unsigned 校准问题。
3. 补上 AFE 采样 skip 的 stale age 保护。
4. 决定 aging 功能是实现还是从工具端隐藏。
5. 按产品暴露面收口 CAN 写参数和 IAP 授权边界。
6. 上硬件做 IAP 升级、参数写入、AFE stale、均衡、休眠唤醒的组合测试。
