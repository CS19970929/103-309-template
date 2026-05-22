# SCI 协议体积优化说明 2026-05-22

## 目标

结合 `103 + 309/Project/Users/Listings/FD_Release.map`，优先处理当前 Release 镜像中最大的协议模块 `sci_upper.o`，在不改变寄存器协议和量产配置的前提下减少 Flash 占用，并降低后续阅读和维护成本。

## map 基线

优化前基线来自 `logs/FD_Release_before_sci_pack_refactor.map`：

```text
Program Size: Code=53676 RO-data=2744 RW-data=808 ZI-data=5256
Total RO  Size: 56420
Total RW  Size: 6064
Total ROM Size: 56664
sci_upper.o Code: 7610
Sci_ACK_0x03_ReadRegs_Data: 1052
Sci_Deal_WrRegs_0x10: 654
```

优化后重新构建 `FD_Release`：

```text
Program Size: Code=52796 RO-data=2744 RW-data=808 ZI-data=5256
Total RO  Size: 55540
Total RW  Size: 6064
Total ROM Size: 55784
sci_upper.o Code: 6738
Sci_ACK_0x03_ReadRegs_Data: 546
Sci_Deal_WrRegs_0x10: 212
```

收益：

- Code 减少 880 字节。
- ROM 减少 880 字节。
- RAM 不变，仍为 6064 字节。
- `sci_upper.o` 代码量减少 872 字节。
- `Sci_ACK_0x03_ReadRegs_Data` 从 1052 字节降到 546 字节。
- `Sci_Deal_WrRegs_0x10` 从 654 字节降到 212 字节。

## 修改内容

1. 新增 `Sci_PutWordBE()`，统一 Modbus 大端 word 写入，替代多个函数里重复的：

```c
t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
```

2. 新增 `Sci_PutZeroWordsBE()`，用于保留协议预留字时批量写入 0，避免展开多段重复代码。

3. 新增 `Sci_RecordBackIndex()` 和 `Sci_PutLatestFaultWords()`，将故障环形记录的倒序索引和打包逻辑集中到一个入口。输出顺序仍保持原来的最近 4 条记录组合方式。

4. 将 `Sci_Deal_WrRegs_0x10()` 中的校准寄存器大段 `case` 列表改为 `Sci_IsCalibPairStart()` 判断：

```text
0x2000 起始、偶数偏移、且不超过 RS485_CMD_ADDR_TEMP_MOS_CALIB_K
```

这与原来只允许 `*_CALIB_K` 起始地址写入 K/B 两个 word 的规则一致。

5. 删除 `Sci_Deal_WrRegs_0x10()` 中已经被前置范围判断覆盖的重复分支。保护参数区和 OtherElement 区仍先走范围判断，协议入口不变。

6. 当前源码中保护参数写入路径已被 `#if 0` 隔离，配套的 `Sci_ApplyProtectSideEffects()` 也保持在同一禁用区间内，避免 Release 构建出现未引用 warning。

## 协议兼容性

- `0x03` 读寄存器响应仍按原有顺序填充 `g_u8SCITxBuff`。
- `0xD000` 基础状态、`0xD100` RTC/故障记录、`0xD200` Cortex fault snapshot、`0xD300` SOC 测试状态窗口位置保持不变。
- `0x10` 写多个寄存器仍保留 SOC 测试、AFE 参数、保护参数、OtherElement、SOC 表、铜损、RTC、SN/版本、IAP 跳转等原有入口。
- 量产配置保持 `PROJECT_CFG_BUILD_PROFILE 0`、`PROJECT_CFG_SOC_TEST_MODE_ENABLE 0`。
- App 链接基址仍为 `0x08004800`，未触碰 IAP/Bootloader 地址规则。

## 验证

执行 Keil Release 构建：

```powershell
& "C:\Keil_v5\UV4\UV4.exe" -b ".\CommomSH367309_16series_103RCT6_C.uvprojx" -t "FD_Release" -o ".\codex_FD_Release_build.log"
```

结果：

```text
".\Objects\FD_Release.axf" - 0 Error(s), 0 Warning(s).
Program Size: Code=52796 RO-data=2744 RW-data=808 ZI-data=5256
```

执行仓库静态检查：

```powershell
py -3.9 tools\project_check.py
```

结果：

```text
OK: 120
Warnings: 0
Errors: 0
```

关键安全检查：

```text
Load Region LR_IROM1 Base: 0x08004800
Execution Region ER_IROM1 Exec base: 0x08004800
Total ROM Size: 55784
Total RW Size: 6064
```

## 第二轮优化记录

继续基于最新 `FD_Release.map` 处理高占用函数，保持协议、错误码语义和量产配置不变。

修改内容：
- `System_ERROR_UserCallback()` 由三段重复 `switch` 改为错误码到 `System_ErrFlag` 字段偏移的表驱动逻辑。
- 保留原语义：普通错误计数递增，`ERROR_TEMP_BREAK` 仍写 `1`，`ERROR_EEPROM_STORE` 上报入口仍不递增，remove/status 命令仍按原字段清零/读取。
- `Sci_WrRegs_0x10_SN_Version()` 的 SN、硬件版本、软件版本写入循环合并到 `Sci_CopyProductIdBytes()`，长度字段和尾部补零规则不变。

第二轮构建结果：
```text
Program Size: Code=52224 RO-data=2768 RW-data=808 ZI-data=5256
Total RO  Size: 54992
Total RW  Size: 6064
Total ROM Size: 55236
FD_Release.bin: 55236 bytes
```

相对第一轮结果：
- Code 继续减少 572 字节。
- RO-data 增加 24 字节，用于错误字段偏移表。
- Total ROM 继续减少 548 字节。
- RAM 仍为 6064 字节。

关键函数尺寸：
```text
System_ERROR_UserCallback: 678 -> 114 bytes
System_ErrorField: 32 bytes
s_u8SystemErrorFieldOffset: 24 bytes
Sci_WrRegs_0x10_SN_Version: 192 -> 106 bytes
Sci_CopyProductIdBytes: 38 bytes
```

累计相对本次优化前基线：
```text
Code:      53676 -> 52224, -1452 bytes
Total ROM: 56664 -> 55236, -1428 bytes
RAM:       6064  -> 6064, unchanged
```

第二轮验证：
```text
".\Objects\FD_Release.axf" - 0 Error(s), 0 Warning(s).
Project check summary:
  OK:       120
  Warnings: 0
  Errors:   0
Load Region LR_IROM1 Base: 0x08004800
Execution Region ER_IROM1 Exec base: 0x08004800
```

## 第三轮优化记录

继续处理 map 中剩余的高占用、低风险重复逻辑。

修改内容：
- `Sci_WrReg_0x06_Reset_CalibCoef()` 改为先解析复位命令对应的校准索引范围，再统一调用 `Sci_ResetCalibCoefIndex()` 完成 K/B 默认值恢复和 EEPROM 写入。
- 0x55AA、0x55AB、0x55AC、0x55AD、0x55AF、0x55B0 的写入目标保持不变。
- 0x55AE 温度校准复位保留历史行为：运行时温度 K/B 置默认值，EEPROM 写入仍使用原有同偏移源索引值，避免在体积优化中改变既有设备行为。
- `InitSystemMonitorData_EEPROM()` 将默认开关、启动标志和系统状态的逐 bit 赋值收敛为命名默认掩码，并注明掩码需要与 `System_Monitor.h` 位序同步。

第三轮构建结果：
```text
Program Size: Code=51716 RO-data=2768 RW-data=808 ZI-data=5256
Total RO  Size: 54484
Total RW  Size: 6064
Total ROM Size: 54728
FD_Release.bin: 54728 bytes
```

相对第二轮结果：
- Code 继续减少 508 字节。
- Total ROM 继续减少 508 字节。
- RAM 仍为 6064 字节。

关键函数尺寸：
```text
Sci_WrReg_0x06_Reset_CalibCoef: 432 -> 164 bytes
Sci_ResetCalibCoefIndex: 60 bytes
InitSystemMonitorData_EEPROM: 330 -> 32 bytes
```

累计相对本次优化前基线：
```text
Code:      53676 -> 51716, -1960 bytes
Total ROM: 56664 -> 54728, -1936 bytes
RAM:       6064  -> 6064, unchanged
```

第三轮验证：
```text
".\Objects\FD_Release.axf" - 0 Error(s), 0 Warning(s).
Project check summary:
  OK:       120
  Warnings: 0
  Errors:   0
Load Region LR_IROM1 Base: 0x08004800
Execution Region ER_IROM1 Exec base: 0x08004800
```

## 第四轮优化记录

继续从 `FD_Release.map` 中选择低风险重复代码，避免触碰 AFE 采样、故障判定、SOC 估算等功能路径。

修改内容：
- `Sci_ACK_0x03_ReadRegs_LCD()` 中 SN、硬件版本、软件版本的 3 段固定长度拷贝收敛为 `Sci_PutBytes()`，输出顺序和每段 32 字节长度保持不变。
- `Sci_ACK_0x03_ReadRegs_Data()` 中连续 8 个保留 word 统一使用 `Sci_PutZeroWordsBE()` 写入，仍然输出 16 个 0 字节，占位数量不变。
- `InitIO()` 和 `InitIO_rtc()` 将 AFIO/GPIOA-GPIOE 的 APB2 时钟使能合并为一次位掩码调用。StdPeriph `RCC_APB2PeriphClockCmd()` 支持外设位掩码，初始化结果与逐个使能一致。

第四轮构建结果：
```text
Program Size: Code=51456 RO-data=2768 RW-data=808 ZI-data=5256
Total RO  Size: 54224
Total RW  Size: 6064
Total ROM Size: 54468
FD_Release.bin: 54468 bytes
```

相对第三轮结果：
- Code 继续减少 260 字节。
- Total ROM 继续减少 260 字节。
- RAM 仍为 6064 字节。

关键函数尺寸：
```text
Sci_ACK_0x03_ReadRegs_Data: 546 -> 406 bytes
Sci_ACK_0x03_ReadRegs_LCD: 200 -> 152 bytes
Sci_PutBytes: 28 bytes
InitIO: 456 bytes
InitIO_rtc: 422 bytes
```

累计相对本次优化前基线：
```text
Code:      53676 -> 51456, -2220 bytes
Total ROM: 56664 -> 54468, -2196 bytes
RAM:       6064  -> 6064, unchanged
```

第四轮验证：
```text
".\Objects\FD_Release.axf" - 0 Error(s), 0 Warning(s).
Project check summary:
  OK:       120
  Warnings: 0
  Errors:   0
Load Region LR_IROM1 Base: 0x08004800
Execution Region ER_IROM1 Exec base: 0x08004800
```

## 第五轮优化记录

继续扩展到 `conf.c` 的初始化和低功耗 IO 配置，处理 map 中较大的 `InitIO()`、`InitIO_rtc()`、`InitWakeUp_NormalMode()`、`IOstatus_Base()`。本轮不触碰 AFE、SOC、故障判定和 SCI 协议语义。

修改内容：
- 新增 `Conf_InitGpioMode()`，统一 GPIO 模式初始化入口，收敛大量重复的 `GPIO_InitStructure` 填充代码。
- 新增 `Conf_InitWakeupInputExti()`，统一唤醒输入 GPIO、EXTI 和 NVIC 配置。触发沿、EXTI line、IRQ 通道和优先级保持原值。
- 将 `InitIO()`、`InitIO_rtc()`、`IOstatus_Base()`、`IOstatus_RTCMode()` 的 GPIO 初始化改为共用入口；各管脚的置位/复位顺序保持不变。
- 将 `InitWakeUp_Base()`、`InitWakeUp_NormalMode()` 中重复的唤醒中断配置改为共用入口。

验证说明：
- 当前主工作区存在未纳入本轮提交的外部 `Sci_Upper.c` 改动，会影响 `0x10` 写寄存器入口和 map 数字。
- 为避免污染验证，本轮在 `.worktrees/conf-size-verify` 中从干净 `HEAD=6a0b055` 建立临时 worktree，只应用本轮 `conf.c` 差异后做全量重建对比。

第五轮全量重建同环境基线：
```text
Program Size: Code=51488 RO-data=2768 RW-data=896 ZI-data=5416
Total RO  Size: 54256
Total RW  Size: 6312
Total ROM Size: 54504
```

第五轮优化后全量重建：
```text
Program Size: Code=50528 RO-data=2768 RW-data=896 ZI-data=5416
Total RO  Size: 53296
Total RW  Size: 6312
Total ROM Size: 53544
FD_Release.bin: 53544 bytes
```

相对同环境全量重建基线：
- Code 减少 960 字节。
- Total ROM 减少 960 字节。
- RAM 保持 6312 字节不变。

关键函数尺寸：
```text
Conf_InitGpioMode: 32 bytes
Conf_InitWakeupInputExti: 92 bytes
IOstatus_Base: 390 -> 210 bytes
InitIO: 456 -> 236 bytes
InitIO_rtc: 422 -> 218 bytes
InitWakeUp_Base: 208 -> 68 bytes
InitWakeUp_NormalMode: 344 -> 104 bytes
```

第五轮验证：
```text
".\Objects\FD_Release.axf" - 0 Error(s), 0 Warning(s).
Project check summary:
  OK:       120
  Warnings: 0
  Errors:   0
Load Region LR_IROM1 Base: 0x08004800
Execution Region ER_IROM1 Exec base: 0x08004800
```
