# D009 老化模式和通讯可靠性说明

## 范围

- D009 补齐 `FactoryAging.c/.h`，保持 CAN App 服务、寄存器读写和老化控制与当前参考分支一致。
- 不迁移 LED、数码管、按键和开关相关差异。
- 当前实物 IAP 工程固定为 `E:\work\a002\new 030\IAP 103CB`。

## 老化模式

- `PROJECT_CFG_FACTORY_AGING_ENABLE` 默认 `1`。
- `PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS` 默认 `259200`，即 72 小时。
- `0x14F80208` 的 `ch=8` 周期广播：
  - byte2：老化状态，`0` 停止/未运行，`1` 运行中，`2` 已完成。
  - byte3~4：剩余分钟，高字节在前，超过 `0xFFFF` 饱和。
- CAN App 独立命令：
  - `0x07 AGING_START`
  - `0x08 AGING_STOP`
  - `0x09 AGING_RESET_TIME`
  - `0x0A AGING_SET_HOURS`
- 老化控制命令 payload 固定为 `0xA9 action CAN_ADRESS_STD_ID`，第三字节校验板端 CAN 地址；comm tool 和 `tools/can_bms_host.py` 都按该格式发送。
- `AGING_SET_HOURS` payload 为 `0xA9 hours CAN_ADRESS_STD_ID`，小时数范围 `1..168`；板端会持久化新老化总时长并重置累计老化时间。
- 老化运行中阻塞普通 RTC 睡眠；低压/过放 deep sleep 判断仍在老化判断前，优先级不变。

## 升级后是否重置老化时间

两个分支都通过 `Project_Config.h` 控制：

```c
#define PROJECT_CFG_UPGRADE_PARAM_RESET_FACTORY_AGING_TIME 0
```

- 默认 `0`：升级后保留现场老化累计时间。
- 改成 `1`：升级后首次启动会调用 `FactoryAging_ResetTimeByHost()` 清零老化累计时间。
- 只要启用该项，必须同步递增 `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION`，否则已经执行过同版本升级策略的板子不会重复执行。

## 通讯偶发 BMS_ERROR

CAN 上位机通过 comm tool 读写 BMS 寄存器时，板端走 `Sci_HostReadWords()` / `Sci_HostWriteWords()` 复用 Modbus 寄存器表。原实现会在本机任一串口短暂忙时直接返回 `RS485_ERROR_CMD_INVALID`，CAN 应答映射成 `BMS_ERROR`，表现为实时监控偶发读取失败。

本次改为：CAN 内部寄存器读写不再因为本机串口收发状态而拒绝；地址、参数、权限仍由原寄存器表校验。

## D009 老化控制失败修正

2026-05-26 现场日志里 `开启老化模式`、`重置老化时间` 返回 `BMS_ERROR`，根因是 D009 板端老化控制校验仍按旧格式要求第三字节等于 `0xA9 ^ action`，而 comm tool 和 CAN host 实际发送的是 `CAN_ADRESS_STD_ID`。已统一为 `0xA9 action CAN_ADRESS_STD_ID`，后续只需要保持上位机、comm tool、板端协议一致。
