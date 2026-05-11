# D010 板级移植说明

本文记录 D010 从 D009 代码基线移植后的硬件 IO、显示逻辑和低功耗交互。D010 后续固定为当前硬件形态，不再用宏保留 Type-C、数码管、74HC595 或 Charlieplexing 兼容分支。

## 产品身份

- D010 固件身份固定为 `BAT_MASTER`，即 `PROJECT_CFG_BAT_TYPE 0`。
- 出厂默认软件版本使用 `BMS_SOFTWARE_VERDION_DEFAULT "D010"`。

## D010 IO 定义

| 功能 | GPIO | 说明 |
| --- | --- | --- |
| SOC25 | `PA3` | SOC >= 1% 点亮 |
| SOC50 | `PA2` | SOC >= 26% 点亮 |
| SOC75 | `PA4` | SOC >= 51% 点亮 |
| SOC100 | `PA7` | SOC >= 76% 点亮 |
| socKey | `PA6` | 灯板开关，低电平按下，`EXTI6` 唤醒 |
| RF_EN | `PA5` | D010 RF 使能脚 |
| CHG_IN | `PA0` | 充电唤醒输入 |
| VBUS ADC | `PA1` | 总压/母线电压 ADC |
| NMOS ADC | `PB1` | MOS 温度 ADC |

已删除或不再使用的旧 IO：

- `GPIO_MCU_WK`
- `GPIO_DC_EN`
- `GPIO_ADC_CUR`
- `GPIO_2727_EN`
- Type-C 相关检测/电流采样引脚
- 数码管、74HC595、Charlieplexing 显示相关引脚

## SOC LED 和 socKey 行为

- SOC LED 为高电平点亮，空闲时关闭并配置为模拟输入，降低休眠漏电。
- 运行状态下短按 `socKey` 显示当前 SOC，默认保持 `PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS = 500`，即 5 秒。
- 运行状态下长按 `socKey` 约 3 秒保存休眠前 SOC，并进入 `DEEP_MODE`。
- 休眠后由 `PA6/EXTI6` 唤醒；短按只预览休眠前 SOC，5 秒后继续回到低功耗；长按约 3 秒判定为有效唤醒。
- 旧 `LedBar_*` 外部接口保留给现有调用点使用，但实现已经固定为 4 个独立 SOC LED，不再执行数码管扫描。

## ADC 和电流逻辑

- ADC 只保留 `PB1` MOS 温度和 `PA1` VBUS 两路扫描。
- Type-C 输出电流变量和 `GPIO_ADC_CUR` 采样链路已经移除。
- SOC 计算只使用 AFE 上报的充放电电流，不再叠加 Type-C 输出电流。

## 低功耗注意事项

- `MCUI_ENI_DI1` 已映射到 `PA6`，用于 socKey 长按唤醒校验。
- STOP/DEEP 休眠前会关闭 SOC LED 输出，仅保留必要唤醒输入。
- D010 不再根据 `GPIO_MCU_WK` 阻止休眠。

## 烧录安全

本工程仍保留 IAP/Bootloader，App 起始地址必须是 `0x08004800`。烧录 App 时优先使用：

```powershell
.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash
```

禁止将 `FD_Debug.bin` 或 `FD_Release.bin` 裸写到 `0x08000000`。
