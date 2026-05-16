# LED_Buzzer 历史模块

## 相关文件

- [LED_Buzzer.c](../../Code/Source/LED_Buzzer.c)
- [LED_Buzzer.h](../../Code/Include/LED_Buzzer.h)

## 模块状态

`LED_Buzzer` 是历史模块，在 Keil 工程中被排除构建，`IncludeInBuild=0`。当前默认运行路径不使用该模块。

## 历史硬件资源

| 信号 | GPIO | 说明 |
| --- | --- | --- |
| LED | PC14 / PC15 | 历史 LED。 |
| Buzzer | PA12 | 与 Heat/Cool Relay 复用。 |
| Emergency key | PA0 | 与充电器/全串唤醒复用。 |

## 维护建议

- 不应在当前默认产品上直接恢复该模块，除非确认 PCB 和资源分配匹配。
- 如果需要蜂鸣器功能，应优先新建清晰的 `Buzzer` 模块，并重新评估 PA12 是否已被加热/冷却占用。
- 保留该模块仅用于理解历史版本，不作为当前运行功能依据。
