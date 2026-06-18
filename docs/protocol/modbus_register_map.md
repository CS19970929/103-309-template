# Modbus 寄存器映射

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/Sci_Upper.h`, `103 + 309/Project/Source/Sci_Upper.c`, `tools/soc_online_monitor.py`, `tools/comm_tool_upgrade_ui.py`
最后更新时间：2026-05-26
未确认事项：完整逐寄存器语义仍需从 `RS485_CMD_RW_E`、参数结构体和上位机读写表继续展开；本文先固定当前源码已确认的地址窗口和高风险入口。

## 1. 协议入口

当前上位机串口协议入口在 `Sci_Upper.c`。

- `InitUSART_CommonUpper()` 初始化 USART 通信口，当前参数为 `19200`, `8N1`, 无硬件流控。
- `Sci_ModbusProtocolFeed()`、`Sci_ModbusProcessFrame()` 负责 Modbus RTU 帧接收和处理。
- 支持读寄存器 `0x03`、单寄存器写 `0x06`、多寄存器写 `0x10`。
- `Can_HDX.c` 的 CAN App 命令也会通过 `Sci_HostReadWords()` / `Sci_HostWriteWords()` 复用同一寄存器访问边界。

## 2. 地址窗口

| 地址/范围 | 当前用途 | 源码证据 | 备注 |
|---|---|---|---|
| `0x1000` 起 | 单寄存器命令区 | `RS485_CMD_RW_E`, `Sci_Deal_WrReg_0x06()` | 包含清校准、清保护记录、清参数、单次写 SOC 等 |
| `0x1008` | 老化累计时间重置 | `RS485_CMD_ADDR_FACTORY_AGING_RESET_TIME` | 写 `0x005A` 后调用 `FactoryAging_ResetTimeByHost()` |
| `0x1009` | 老化总时长设置 | `RS485_CMD_ADDR_FACTORY_AGING_SET_HOURS` | 写 `1..168` 小时后调用 `FactoryAging_SetDurationHoursByHost()`，并重置累计时间 |
| `0x1100` 起 | 系统功能开关命令 | `RS485_CMD_ADDR_SYSFUNC_ONOFF_*` | 具体功能保留/删除需用户确认 |
| `0x2000` 起 | 校准参数 | `RS485_ADDR_RW_CALIB`, `RS485_CMD_ADDR_VC1CALIB_K` | 电压、电流、温度等 K/B 参数 |
| `0x2100` 起 | 保护参数 | `RS485_ADDR_RW_PORTECT`, `RS485_CMD_ADDR_VCELL_OVP_FIRST` | OVP/UVP/OCP/温度/SOC 保护参数 |
| `0x2200` 起 | SOC/RTC/其他参数 | `RS485_ADDR_RW_OTHER`, `RS485_CMD_ADDR_SOC_VOLTAGE1` | SOC 表、铜损、RTC、容量等 |
| `0x2300` 起 | 均衡/睡眠/系统参数 | `RS485_ADDR_RW_OTHER_CANADD`, `RS485_CMD_ADDR_BALANCE_OV` | 名称中仍有 CANADD 历史痕迹，需后续重命名确认 |
| `0x2500` | SOC 注入测试样本 | `RS485_CMD_ADDR_SOC_TEST_SAMPLE` | 当前量产源码未启用 SOC 注入测试入口 |
| `0xC000` | LCD/独立只读块起点 | `RS485_ADDR_RO_LCD` | 与 `0xD000` 主只读块独立处理 |
| `0xC001` | RTC/出厂相关只读入口 | `RS485_ADDR_RO_FA_RTC` | 具体语义需继续逐项核对 |
| `0xC002` | BMS 序列号/硬件版本/软件版本读取 | `RS485_ADDR_SN_READ`, `tools/comm_tool_upgrade_ui.py` | 上位机要求读取 48 个寄存器并在实时监控底部显示 |
| `0xC008` | 事件记录读取 | `RS485_ADDR_EVENT_RECORD` | 每条事件日志映射为一个 Modbus register |
| `0xC080` | 老化状态只读窗口 | `RS485_ADDR_AGING_STATUS` | 5 words：`state`, `remaining_minutes`, `remaining_seconds_hi`, `remaining_seconds_lo`, `duration_hours` |
| `0xD000` | 主实时只读窗口 | `RS485_ADDR_RO_START0`, `Sci_ACK_0x03_ReadRegs_Data()` | 当前注释为 63 个 `g_stCellInfoReport` words |
| `0xD100` | RTC/故障/系统状态只读窗口 | `RS485_ADDR_RO_START1` | 当前拼接在主只读 buffer 后，约 33 words |
| `0xD200` | Cortex fault snapshot | `RS485_ADDR_RO_START2` | `D200` reason, `D201` inverse |
| `0xD300` | SOC 测试状态只读窗口 | `RS485_ADDR_RO_SOC_TEST` | 16 words；量产固件读到 unsupported 是正常隔离结果 |
| `0xFFFD` | Flash/IAP 连接命令 | `RS485_CMD_ADDR_FLASH_CONNECT`, `Sci_WrRegs_0x10_FlashConnect()` | 调用 `AppUpgrade_RequestIap()` 后触发进入 IAP |

## 3. 当前读窗口规则

`Sci_Deal_ReadRegs_0x03()` 不是按绝对地址直接索引，而是把多个窗口折算到同一个内部只读缓冲区：

1. `0xD000` 映射到主只读窗口偏移 0。
2. `0xD100` 映射到主窗口后追加的 RTC/故障/状态区。
3. `0xD200` 映射到 fault snapshot 两个 word。
4. `0xD300` 映射到 SOC 测试状态窗口。
5. `0xC000` 到 `0xD000` 之间的独立只读块有单独范围校验。

这意味着后续不能随意移动 `0xD000`、`0xD100`、`0xD200`、`0xD300`，否则会破坏上位机和测试脚本。

## 4. 当前写权限边界

- 写入口受 `PROJECT_CFG_HOST_WRITE_ENABLE` 控制。
- 读写函数会做地址窗口和长度校验，错误时返回 Modbus 异常。
- SOC 表运行时写入功能已删除；相关写入口固定返回 Modbus 异常，读表仍返回当前编译期 OCV 表以保持协议窗口可读。
- `0x1005` 单次写 SOC 是对外可见行为，CAN App 和上位机工具都依赖。
- `0x1008` 老化累计时间重置必须写 guard `0x005A`，避免误触发。
- `0x1009` 老化总时长设置范围固定 `1..168` 小时；成功后板端会重置累计老化时间并进入运行状态。
- `0xFFFD` 进入 IAP 属于高风险写入口，必须保留确认机制。

## 5. 后续维护规则

1. 新增或修改寄存器必须同步更新本文和 `docs/design/protocol_design.md`。
2. 任何 `0xD000`、`0xC002`、`0xD300`、`0x1005`、`0xFFFD` 相关改动都要做上位机回归。
3. 完整逐寄存器表建议后续从 `Sci_Upper.h` 自动生成，避免手工表漂移。
