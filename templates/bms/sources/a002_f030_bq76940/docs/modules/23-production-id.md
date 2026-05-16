# 生产 ID 与版本信息

## 相关文件

- [ProductionID.c](../../Code/Source/ProductionID.c)
- [ProductionID.h](../../Code/Include/ProductionID.h)
- [DataDeal.h](../../Code/Include/DataDeal.h)
- [EEPROM.c](../../Code/Source/EEPROM.c)

## 模块职责

`ProductionID` 负责产品序列号、硬件版本、软件版本等生产信息的内部 Flash 读写，并通过通信协议对外提供查询和写入能力。

## 默认值

| 字段 | 默认值 |
| --- | --- |
| Serial number | `hanstar` |
| Hardware version | `hanstar` |
| Software version | `a002-c063v1p0` |

## 内部 Flash 逻辑地址

| 地址 | 内容 | 最大长度 |
| --- | --- | --- |
| `830` | Serial number | 32 bytes |
| `870` | Hardware version | 32 bytes |
| `910` | Software version | 32 bytes |

## 主调度

`App_ProID_Deal()` 在主循环中执行：

1. 启动后从内部 Flash 读取生产信息。
2. 检测上位机写入标志。
3. 将更新后的字符串写回内部 Flash。

## 与通信协议关系

`Sci_Upper` 中的 `0xFFF0`、`0xFFF1`、`0xFFF2` 地址用于读写序列号、硬件版本和软件版本。

## 维护建议

- 版本字符串是售后和升级判断的重要信息，发布前应确认默认值与实际固件版本一致。
- 写入长度应严格限制，避免覆盖相邻内部 Flash 逻辑区域。
- 如果软件版本参与升级兼容判断，建议增加结构化版本规则，而不是只依赖自由字符串。
