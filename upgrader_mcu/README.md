# STM32F103C8 CAN 升级器 MCU 实现说明

本目录实现专用升级器 MCU 的核心逻辑和 STM32F103C8 适配层。默认硬件连接为 USART1 `PA9/PA10` 接 PC USB 转串口，CAN1 `PA11/PA12` 接 BMS，CAN 波特率 250 kbit/s，串口波特率 115200。

## 1. 当前已实现能力

| 能力 | 状态 |
| --- | --- |
| PC 串口二进制协议解析 | 已实现 |
| PC 串口响应封包 | 已实现 |
| 飞道 29bit CAN ID 封包/解包 | 已实现 |
| BMS 广播快照缓存 | 已实现 |
| CAN_OBJECT_READ 通用对象读取 | 已实现 |
| CAN_OBJECT_WRITE 通用对象写入 | 已实现 |
| PARAM_READ 参数表读取 | 已实现 |
| PARAM_WRITE 读-改-写-读回前置写入 | 已实现 |
| ENTER_BMS_IAP 进入 BMS IAP 命令 | 已实现 |
| PDF V1.6 第七节 IAP 起始帧 | 已实现 |
| PDF V1.6 第七节长包起始/数据/结束 | 已实现 |
| 每长包 ACK 和最终完成 ACK 处理 | 已实现 |
| PC 串口上位工具 | 已实现 |
| 主机侧 C 单元测试 | 已实现 |
| STM32F103C8 固件适配层 | 已实现 |
| ARMCC 构建脚本 | 已实现 |

## 2. 目录结构

```text
upgrader_mcu/
  core/
    upg_core.c/.h        升级器 MCU 主状态机和命令分发
    upg_serial.c/.h      PC 串口帧编码/解析
    upg_feidao.c/.h      飞道 CAN 扩展帧封包/解包
    upg_params.c/.h      参数表和参数读写编码
    upg_crc16.c/.h       CRC16-Modbus
    upg_utils.c/.h       大端读写工具
    upg_protocol.h       命令码、错误码、节点、地址常量
  tests/
    upgrader_core_test.c 主机侧协议测试
  stm32f103c8/
    board_main.c          STM32F103C8 主循环
    board_uart.c/.h       USART1 串口收发
    board_can.c/.h        CAN1 扩展帧收发
    board_config.h        板级默认参数
    UPG_F103C8.sct        64KB Flash scatter
```

PC 侧工具：

```text
tools/upgrader_mcu_host.py
tools/start_upgrader_mcu_host.ps1
tools/build_upgrader_mcu_f103c8.ps1
tools/run_upgrader_mcu_tests.py
```

## 3. STM32F103C8 接入方式

当前适配层默认采用：

| 方向 | 接口 |
| --- | --- |
| PC -> 升级器 MCU | USART1 `PA9/PA10` + USB 转串口芯片，115200 |
| 升级器 MCU -> BMS | CAN1 默认 `PA11/PA12`，250 kbit/s |

如果使用 STM32 原生 USB CDC，则 USB 会占用 PA11/PA12，CAN1 必须重映射到 PB8/PB9。当前实现先按 USB 转串口芯片方案走，避免和 CAN 默认引脚冲突。

构建升级器 MCU 固件：

```powershell
.\tools\build_upgrader_mcu_f103c8.ps1 -Clean
```

产物输出：

```text
upgrader_mcu/build/f103c8/UPG_F103C8.bin
upgrader_mcu/build/f103c8/UPG_F103C8.axf
upgrader_mcu/build/f103c8/UPG_F103C8.map
```

硬件适配层只需要实现：

```c
static int board_can_tx(void *user, const UpgCanFrame *frame);
static int board_serial_tx(void *user, const uint8_t *data, uint16_t len);
static void board_reset(void *user);
```

然后初始化：

```c
UpgCore core;
UpgHal hal = {
    board_can_tx,
    board_serial_tx,
    board_reset,
    board_user
};

UpgCore_Init(&core, &hal);
```

主循环：

```c
while (1) {
    UpgCore_Tick(&core, board_get_ms());

    if (uart_rx_available()) {
        uint8_t buf[64];
        uint16_t n = uart_read(buf, sizeof(buf));
        UpgCore_OnSerialBytes(&core, buf, n);
    }

    if (can_rx_available()) {
        UpgCanFrame frame;
        board_can_read(&frame);
        UpgCore_OnCanFrame(&core, &frame);
    }
}
```

## 4. PC 串口工具

默认串口参数：`115200, 8N1, no flow control`。如果后续硬件时钟改为更高主频，可同步修改 `upgrader_mcu/stm32f103c8/board_config.h` 和 PC 工具 `--baud`。

列出串口：

```powershell
.\tools\start_upgrader_mcu_host.ps1 -Mode detect
```

读取升级器信息：

```powershell
.\tools\start_upgrader_mcu_host.ps1 -Mode info -Port COM8
```

读取 BMS 广播缓存：

```powershell
.\tools\start_upgrader_mcu_host.ps1 -Mode snapshot -Port COM8
```

读取飞道 CAN 对象：

```powershell
.\tools\start_upgrader_mcu_host.ps1 -Mode read-object -Port COM8 -Index 0x02 -Chd 0x04
```

写飞道 CAN 对象：

```powershell
.\tools\start_upgrader_mcu_host.ps1 -Mode write-object -Port COM8 -Index 0x20 -Chd 0x00 -Data "10 68 09 C4 00 00 00 00"
```

读取参数：

```powershell
.\tools\start_upgrader_mcu_host.ps1 -Mode read-param -Port COM8 -ParamId 0x1001
```

写参数：

```powershell
.\tools\start_upgrader_mcu_host.ps1 -Mode write-param -Port COM8 -ParamId 0x1001 -RawValue 4200 -Confirm
```

升级 dry-run：

```powershell
.\tools\start_upgrader_mcu_host.ps1 -Mode upgrade-dry-run -Bin "103 + 309\Project\Users\Objects\FD_Release.bin"
```

通过升级器 MCU 升级 BMS：

```powershell
.\tools\start_upgrader_mcu_host.ps1 -Mode upgrade -Port COM8 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -EnterIap
```

## 5. 参数表

当前 `upg_params.c` 已实现参数表机制和示例参数：

| param_id | CAN Index | Chd | Offset | 类型 | 写入 |
| --- | --- | --- | --- | --- | --- |
| `0x1001` | `0x20` | `0x00` | 0 | U16 | 允许，需确认 |
| `0x1002` | `0x20` | `0x00` | 2 | U16 | 允许，需确认 |
| `0x1003` | `0x20` | `0x01` | 0 | U16 | 允许，需确认 |
| `0x1004` | `0x20` | `0x01` | 2 | U16 | 允许，需确认 |
| `0x1101` | `0x02` | `0x02` | 1 | U8 | 只读 |

这些参数 ID 是升级器 MCU 内部稳定 ID。当前 BMS App 已支持 `0x1001..0x1004` 对应的单节过压、单节低压及其滤波时间读写；后续需要继续把温度参数、过流参数等补入此表。通用 `read-object/write-object` 已经可用于协议联调。

## 6. 测试

主机侧测试命令：

```powershell
py -3.9 tools\run_upgrader_mcu_tests.py
```

当前测试覆盖：

1. 串口 `CAN_OBJECT_READ` 到 CAN 读对象，再处理 BMS ACK。
2. `PARAM_WRITE` 的读-改-写流程，确认同一 8 字节对象其他字段不被破坏。
3. `UPGRADE_PREPARE` 发送 PDF 第七节 A-0 起始帧并处理 A-1。
4. `UPGRADE_PACKET_DATA/COMMIT/FINISH` 发送长包起始、数据帧、结束帧，并处理完成 ACK。
5. BMS 广播缓存和 `READ_BMS_SNAPSHOT`。

## 7. 后续联调清单

1. 烧录 `upgrader_mcu/build/f103c8/UPG_F103C8.bin` 到升级器 MCU `0x08000000`。
2. PC 运行 `.\tools\start_upgrader_mcu_host.ps1 -Mode info -Port COMx`，确认能读到 `STM32F103C8-UPG`。
3. 接入 BMS CAN，先用 `read-object` 读取 `Index=0x02 Chd=0x04`。
4. 再执行 `snapshot`，确认广播缓存有电池信息。
5. 用 `read-param/write-param` 验证 `0x1001..0x1004` 示例保护参数读写。
6. 最后执行 `upgrade-dry-run` 和真实 `upgrade -EnterIap`。

核心规则：硬件工程不要改业务协议逻辑，协议逻辑集中在 `core` 目录，便于主机测试持续覆盖。

`ENTER_BMS_IAP` 兼容当前 BMS App 已有的标准帧命令：升级器向标准 ID `0x60` 发送 `A5 5A 02 C3 3C 00 CRC16`，等待标准 ID `0x61` ACK。BMS 复位进入 IAP 后，实际固件传输严格使用 PDF V1.6 第七节扩展帧协议。
