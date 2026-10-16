# 当前板低功耗框架验证记录

日期：2026-10-16。固件源码：`3626ff1`，另包含用户已有的 `DataDeal.h`（SNum=6）和 `conf.h`（IWDG 关闭）修改；这两处未纳入本次提交。

## 构建、下载和实物

- Keil Target 1，ARMCC 5.06 update 7，AXF：`103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.axf`。
- AXF SHA256：`C58A0752EBF501496B586AF9D8AC6B9788300FC2DB31C19E4966428DD175AEC3`。
- Code=45132、RO=2972、RW=1356、ZI=6224，0 errors、7 处现有警告。各轮构建关闭 BIN 后处理，完成后恢复设置，没有生成 BIN。
- Keil 下载：`Erase Done.Programming Done.Verify OK.Application running ...`。
- ST-Link V2J37S7；目标电压约 3.29 V；DBGMCU_IDCODE=`0x20036410`，Flash size register=`0x0080`（128 KiB）。ID 不等于已核实芯片品牌/丝印。
- MAP：`PWR_EnterSTOPMode=0x20000001`（Thumb 地址），例程继续由 scatter loading 放入 RAM；APP 起始地址保持 `0x08004800`。

## 5 分钟周期附着观察

按用户描述，通信盒持续连接 USART1、未主动通信。GDB 每次读取后分离，通过 Tcl 显式 resume 并确认 `curstate=running`。整个有效观察没有 reset 或修改业务变量。

| 相对时间 s | STOP 尝试数 | RTC 周期数 | 无来源返回数 |
| ---: | ---: | ---: | ---: |
| 4.4 | 10 | 9 | 0 |
| 45.7 | 21 | 20 | 0 |
| 87.0 | 31 | 29 | 1 |
| 128.5 | 42 | 40 | 1 |
| 169.8 | 53 | 50 | 2 |
| 211.2 | 64 | 60 | 3 |
| 252.6 | 74 | 69 | 4 |
| 294.0 | 85 | 80 | 4 |
| 304.5 | 88 | 82 | 5 |

全部快照：`state=LP_STOP`、`target=LP_TARGET_RTC`、`block_mask=0`、`wake_mask=LP_WAKE_RTC`、`fault_mask=0`；`external_wake_count/restore_count/abort_count/RTC_ExtComCnt/uart1_errors/uart2_errors/hardfault_count` 均为 0。PC 为 `0x20000028`（RAM 例程 WFI）。5 次无来源返回均重新进入 STOP，没有触发重试上限或恢复 RUN；它们发生于反复附着期间，不能据此认定无调试器时也会出现。

重要证据边界：固件 `debug_profile=0`，但 OpenOCD 的 `stm32f1x.cfg` examine-end 自动写入 `DBGMCU_CR=0x307`。所以以上属于带 STOP debug 保持的功能验证，不能当作 DBG_STOP=0 或电流验收。

此前一轮脚本仅 detach，没有确认 resume，CPU 留在 halted，其记录未用于稳定性结论；正式工具已经修正此行为。

## 有界单步

在自然 RTC 返回后的 `BoardLowPower_CaptureWake()` 硬件断点停住，设置 HardFault 捕获断点，执行 256 条 `stepi`。覆盖 RTC RSF 同步、alarm 捕获、原 PRIMASK 恢复和进入 RTC_SAMPLE，结束于 `BoardLowPower_Snapshot()`。

- `g_hardfault_record.count=0`，SCB CFSR/HFSR 均为 `0x00000000`。
- 未命中 HardFault 断点；删除全部测试断点后分离。
- 这是 GDB/ST-Link 的有限路径单步证据，不等同于已复现并完全排除用户原来的 Keil USART1 IRQ 单步问题。

## DBG_STOP=0 补测的阻塞

单步结束后读取实际 `DBGMCU_CR=0x307`，关闭 OpenOCD 的自动事件并清除 `0x307` 位。随后尝试真实 STOP 周期附着，出现 SWD `Fail reading CTRL/STAT register`，无法取得可信 RAM 记录。

恢复尝试：OpenOCD under-reset（`srst_only srst_nogate connect_assert_srst`）未能连接；Keil 对同一已验证 AXF 的恢复下载也失败，日志为 `Internal command error / Flash Download failed - Target DLL has been cancelled`。恢复下载没有得到擦除、编程或校验成功记录，不能宣称已重新烧录或恢复运行。

需要人为按板上 reset 或断电重上电，保持 ST-Link 连接；最好接好 NRST。恢复后应在 RUN 时预置唤醒后硬件断点，再验证 DBG_STOP=0。当前证据只能确认调试通道不可访问，不能由此判断 MCU 已经 HardFault 或 RTC 没有唤醒。

仍未实测：物理充/放电退出、各外部引脚真实边沿/脉宽、IWDG 开启、Keil 原操作复现、脱离调试器后的电流和长时间/温度条件。不得将本记录作为量产功耗或保护验收。

原始日志在 `%LOCALAPPDATA%/CodexTemp/103-309-template/`：`round3-keil-build-final.log`、`framework-keil-flash.log`、`framework-board-observe-resumed/`、`framework-step.log`、`framework-debug-recover.log`、`framework-keil-recover.log`。
