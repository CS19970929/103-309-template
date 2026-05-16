# BMS 通用模板架构口径

## 固定结论

通用模板的应用层逻辑以当前 `103 + 309/Project/Source` 为唯一基线。旧 A002 项目不再作为应用层流程参考，只作为 `STM32F030` MCU port、`BQ76940` AFE port、F0 Keil/scatter/Flash 地址和硬件资源参考。

这条规则用于避免后续生成器把旧项目的 SOC、低功耗、软件保护、通信状态机继续复制到新项目里，导致模板分叉。

## 分层目标

| 层级 | 来源 | 职责 |
| --- | --- | --- |
| Application Core | 当前项目 | 主循环、运行时序、SOC、CAN、低功耗、Storage、显示、通信、保护 owner 入口。 |
| Board Profile | `target_profiles.json` + 配置头 | MCU/AFE/保护策略/功能开关/地址边界。 |
| MCU Port | 当前 F103 + A002 F0 参考 | startup、system clock、interrupt、Flash/IAP、GPIO、RTC、CAN/UART、watchdog。 |
| AFE Port | 当前 SH367309 + A002 BQ76940 参考 | 电压/电流/温度采样、MOS 控制、硬件保护状态、AFE latch 清除。 |
| Generated Project | 生成器输出 | Keil 工程、scatter、配置头、源码目录、生成报告和检查脚本。 |

## 应用层必须保持的当前项目行为

- 主循环和任务节拍以当前项目为准，不继承 A002 `main.c` 调度顺序。
- SOC 以当前项目已收口的体验策略为准，后续只通过 port 输入适配电压、电流、RTC 和存储。
- 低功耗以当前项目 `LowPower_*` / `rtc_sleep` / CAN RTC 唤醒闭环为准，旧 A002 `SleepDeal` 只作为 F0 低功耗寄存器参考。
- 当前项目 CAN/RTC 需求保持：RTC 唤醒后执行 CAN 发送闭环，再决定是否继续休眠。
- 存储统一为内部 Flash，不恢复外部 EEPROM。
- 保护 owner 通过 profile 切换：`SH367309` 默认硬件保护主导；`BQ76940` 或其他 AFE 可启用软件保护 owner，但接口仍接入当前应用层框架。

## 生成器输入应覆盖

| 输入项 | 示例 |
| --- | --- |
| MCU | `stm32f103`、`stm32f030` |
| AFE | `sh367309`、`bq76940` |
| 保护策略 | `afe_hardware_only`、`mcu_software`、`hybrid` |
| 存储 | 当前只允许 `internal_flash` |
| 通信 | `can`、`rs485`、`uart_client` |
| 低功耗 | `rtc_can_wakeup`、`stop_sleep`、`no_low_power` |
| 显示 | `ledbar`、`none` |
| 可选功能 | `heat`、`factory_aging`、`soc_test_mode` |

## 禁止事项

1. 禁止把 A002 旧应用层 `SOC.c`、`SleepDeal.c`、`Fault.c`、`Sci_Upper.c` 作为新模板主线复制。
2. 禁止跨 profile 混用 F0/F1 App、IAP、Storage 地址。
3. 禁止恢复外部 EEPROM 后端作为默认模板能力。
4. 禁止在应用层散落 `#ifdef BQ76940`、`#ifdef SH367309` 这类 AFE 判断；必须走 AFE port。
5. 禁止生成器直接复制未知旧文件；所有文件必须来自明确的应用层基线或 port profile。

## 当前下一步

1. 在当前项目源码上继续收口通用接口：`Project_Target.h`、`Project_Protection.h`、`Project_Features.h`、`Platform_Port.h`、`BmsModel.h`、`BoardControl.h`、`AfeService.*`。
2. `BoardControl.c` 承接当前板级 MOS/工厂模式控制，后续换 AFE/板型时只替换 board control port，不改 `main.c`。
3. 把 A002 中可用的 F0 startup、system clock、Flash/IAP、RTC、GPIO、BQ76940 I2C/寄存器访问整理成 port 参考清单。
4. 当前生成器已支持 `list/show/dry-run/generate`，但 `generate` 只对 `fd_103_309` 这种 canonical 应用基线开放。
5. F0/BQ76940 继续按 port reference 整理，待 MCU port、AFE port、Keil/scatter 物化规则完成后再开放生成。
