# BMS App CAN 服务协议

## 作用

BMS App CAN 服务用于 comm tool 在正常 App 运行时读取状态、写保护参数，以及让 App 进入 IAP。它不是升级数据传输协议，真正固件写入由 BMS IAP 完成。

## 当前分支适配说明

- 以 `codex/d009-production-code` 的 CAN App 服务帧格式和 CAN-IAP 帧格式为准。
- `ENTER_IAP` 按 d009 的 SRAM mailbox 方式进入 IAP：App 写 `0x20004FE0` mailbox，ACK 后延时复位；不再把 `FLASH_ADDR_UPDATE_FLAG` 作为新的升级门闩。
- 当前项目保护参数不同于 d009：主保护参数表为 `0x2100..0x2140`，共 65 word，13 组，每组 5 word。CAN 单寄存器写入保护参数时，板端会按当前项目规则自动合成所在 5-word 组写入，保留原 `Sci_WrRegs_0x10_Protect()` 的写标志和副作用。
- 当前项目没有 d009 的 `FactoryAging` 模块；老化命令 `0x07..0x0A` 在本分支板端返回明确拒绝，不做静默超时。

## CAN 帧

第一阶段使用标准帧，保持与当前 App 命令骨架兼容。

| 方向 | 标准帧 ID | 说明 |
| --- | --- | --- |
| comm tool -> BMS App | `(CAN_ADRESS_STD_ID << 7) | 0x60` | App 服务请求 |
| BMS App -> comm tool | `(CAN_ADRESS_STD_ID << 7) | 0x61` | App 服务响应 |

请求固定 8 字节：

| 字节 | 含义 |
| --- | --- |
| 0 | `0xA5` |
| 1 | `0x5A` |
| 2 | 命令 |
| 3~5 | 参数 |
| 6~7 | 前 6 字节 CRC16-Modbus，高字节在前 |

响应固定 8 字节：

| 字节 | 含义 |
| --- | --- |
| 0 | `0x5A` |
| 1 | `0xA5` |
| 2 | 原命令 |
| 3 | 状态码，`0x00` 成功 |
| 4~5 | 返回值 |
| 6~7 | 前 6 字节 CRC16-Modbus，高字节在前 |

块读数据帧也使用 BMS App 响应 ID，但字节 2 固定为 `0x86`，字节 3 为序号，字节 4~5 为寄存器值：

| 字节 | 含义 |
| --- | --- |
| 0 | `0x5A` |
| 1 | `0xA5` |
| 2 | `0x86` |
| 3 | 从 `0` 开始的寄存器序号 |
| 4~5 | 寄存器值，高字节在前 |
| 6~7 | 前 6 字节 CRC16-Modbus，高字节在前 |

## 已实现命令

| 命令 | 参数 | 返回 | 说明 |
| --- | --- | --- | --- |
| `0x01 GET_STATUS` | `00 00 00` | `SOC, SOH` | 读取基础状态，兼容早期 comm tool |
| `0x02 ENTER_IAP` | `C3 3C can_addr` | `08 48` | 写升级标志并延时复位进入 IAP |
| `0x03 READ_REG` | `addr_hi addr_lo 00` | `value_hi value_lo` | 读取一个 BMS 串口寄存器 |
| `0x04 WRITE_PREP` | `addr_hi addr_lo value_hi` | `addr_hi addr_lo` | 写单个寄存器第一帧，暂存地址和高字节 |
| `0x05 WRITE_COMMIT` | `addr_hi addr_lo value_lo` | `00 00` | 写单个寄存器第二帧，校验地址一致后提交 |
| `0x06 READ_BLOCK` | `addr_hi addr_lo count` | `count, 00` 后续 `0x86` 数据帧 | 一次读取 `1..120` 个连续寄存器，供 UI 状态页和实时监控使用 |
| `0x07 AGING_START` | `A9 51 can_addr` | `aging_state, remaining_hours` | 单独开启老化模式；若当前已完成，则清零累计时间并开启新一轮 |
| `0x08 AGING_STOP` | `A9 50 can_addr` | `aging_state, remaining_hours` | 单独关闭老化模式，并提前结束本轮老化时间；板端持久化完成状态，剩余时间清零 |
| `0x09 AGING_RESET_TIME` | `A9 5A can_addr` | `aging_state, remaining_hours` | 单独重置老化模式累计时间 |
| `0x0A AGING_SET_HOURS` | `A9 hours can_addr` | `aging_state, remaining_hours` | 修改老化总时长，单位小时，范围 `1..168`；修改成功后自动重置累计老化时间 |

## 常用上位机功能

- 写 SOC 必须在原 `BMS_CommTool_Upgrade_UI.exe` 里作为单独常用功能展示，位置为 `其它功能 -> 常用功能 -> 写SOC`；命令行调试入口可使用 `app-write-soc`。
- 写 SOC 底层复用 `WRITE_PREP/WRITE_COMMIT` 写寄存器 `0x1005 RS485_CMD_ADDR_SET_ONCE_SOC`，范围固定 `0..100`，不要求用户手动输入寄存器地址。
- 老化模式三个动作必须在原 UI 里独立实现和展示：`开启老化模式`、`关闭老化模式`、`重置老化时间`，不能合并成一个带 action 参数的通用入口；其中 `关闭老化模式` 的语义是提前结束本轮老化时间，返回状态应为完成且剩余时间为 0。
- 原 UI 必须单独提供 `读取老化时间`，通过 comm tool 串口命令 `0x13 BMS_AGING_STATUS` 等待并解析 `0x14F80208` 周期广播，把 `ch=8` 的老化状态和剩余分钟显示给用户。
- 原 UI 必须单独提供 `修改老化时间`，用户输入单位为小时；底层通过 comm tool 串口命令 `0x14 BMS_AGING_SET_HOURS` 转成 CAN App `0x0A AGING_SET_HOURS`，板端持久化新时长并清零已累计时间。
- 老化启停/重置命令使用 `A9 + action + can_addr` 防误触发；修改老化时长使用 `A9 + hours + can_addr`；`can_addr` 必须等于板端 `CAN_ADRESS_STD_ID`。
- 原 UI 的实时监控界面最底部必须显示 BMS 序列号、硬件版本、软件版本。读取来源是现有 `BMS_READ` 读 `0xC002`，长度 48 个寄存器，数据布局为 32 字节序列号、32 字节硬件版本、32 字节软件版本。

## 周期广播补充

扩展帧基址仍为 `0x14F80200`，老化剩余时间放在现有通道 `ch=8` 的 5 秒广播帧中：

| 字节 | 含义 |
| --- | --- |
| 0~1 | 出厂容量 raw，保持原兼容 |
| 2 | 老化状态：`0` 停止/未运行，`1` 运行中，`2` 已完成 |
| 3~4 | 老化剩余分钟，高字节在前；超过 `0xFFFF` 时饱和 |
| 5~7 | 生产日期：年、月、日，保持原兼容 |

## 状态码

| 状态码 | 含义 |
| --- | --- |
| `0x00` | 成功 |
| `0x01` | 命令不支持 |
| `0x02` | 参数错误或寄存器地址非法 |
| `0x05` | App 进入 IAP 请求失败 |
| `0x07` | 无写权限。当前项目 Release 默认 `PROJECT_CFG_HOST_WRITE_ENABLE=1`，正常不应返回该状态 |
| `0x08` | BMS 寄存器处理失败 |

## 寄存器读写规则

- CAN App 服务不重新定义寄存器地址，统一复用 `Sci_Upper.c` 原有串口寄存器表。
- 读寄存器调用 `Sci_HostReadWords()`，使用原 `0x03` 读寄存器校验和返回数据生成逻辑。
- comm tool 读取多个寄存器时优先使用 `READ_BLOCK`。BMS App 收到块读后只调用一次 `Sci_HostReadWords()`，再每 10ms 发送一个 `0x86` 数据帧，避免升级后 App 刚恢复时被大量单寄存器 CAN 请求打满。
- 写寄存器调用 `Sci_HostWriteWords()`，使用原 `0x06/0x10` 写寄存器权限、范围、副作用处理。
- CAN 接收中断只缓存请求帧；真正寄存器处理在 `App_Can()` 主循环中执行，避免在中断里操作 Flash/EEPROM 或串口缓存。
- 如果任一串口通道正在收发，`Sci_HostReadWords()` / `Sci_HostWriteWords()` 会拒绝本次 CAN 寄存器访问，避免和原串口响应缓存冲突。

## 低功耗关系

运行态 `InitCan()` 打开 CAN 收发器，App 服务命令不依赖周期广播后的短窗口。进入 RTC STOP 前 `Can_PrepareSleep()` 会关闭 CMNT，RTC 周期唤醒期间不主动广播 CAN；外部唤醒或退出低功耗恢复后，`InitCan()` 重新打开 CMNT，再恢复 CAN App 服务。
