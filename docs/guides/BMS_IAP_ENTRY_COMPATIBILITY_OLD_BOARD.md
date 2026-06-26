# BMS 串口 IAP 入口条件与旧板兼容性分析

日期：2026-06-24  
范围：当前 BMS App、参考 IAP `E:\work\a002\new 030\IAP 103CB`、新上位机直连串口升级流程、旧串口上位机升级流程。

## 1. 结论

旧板“升级协议兼容”不代表“一定能进入 IAP”。串口 IAP 升级至少分成两个阶段：

1. **App 阶段进入 IAP**：BMS App 必须收到进入 IAP 的 Modbus 命令，写入 IAP 入口标志，然后复位。
2. **IAP 阶段接管升级**：Bootloader/IAP 复位后必须识别入口标志并停留在 IAP，然后才开始接收 `0xFFFD/0xFFFE/0xFFFF` 升级协议。

当前项目中，App 进入 IAP 使用的是 **SRAM mailbox**：

```text
地址：0x20004FE0
magic：0x49415031
request：0x5AA55AA5
```

如果旧板上烧录的是更早版本 IAP，并且该 IAP 只识别旧的 Flash 标志：

```text
FLASH_ADDR_UPDATE_FLAG = 0x0801F800
FLASH_TO_IAP_VALUE    = 0x00AB
```

那么现象会是：App 能收到进入 IAP 命令甚至复位，但旧 IAP 看不到自己期望的 Flash 标志，会直接跳回 App，最终表现为“协议兼容，但进入不了 IAP，无法升级”。

此外，当前 App 串口波特率是 `115200`，参考 IAP 串口波特率是 `19200`。如果上位机/comm tool 在 App 阶段和 IAP 阶段使用同一个 BMS 侧波特率，也可能导致“已复位但 IAP 无响应”。

## 2. 当前源码中的进入 IAP 条件

### 2.1 App 必须收到正确的 Modbus 写多寄存器命令

当前 BMS App 处理进入 IAP 的入口在：

- `103 + 309/Project/Source/Sci_Upper.c`
- `RS485_CMD_ADDR_FLASH_CONNECT = 0xFFFD`
- `Sci_WrRegs_0x10_FlashConnect()`

App 只在以下条件成立时请求进入 IAP：

```text
Modbus function = 0x10
start address   = 0xFFFD
register count  = 1
CRC             = 正确
```

典型 App 进入 IAP 请求帧：

```text
01 10 FF FD 00 01 02 00 01 7D 72
```

说明：

- `01`：App Modbus 地址。当前源码宏 `RS485_SLAVE_ADDR` 是 `0x01`，但旧项目或旧板如果改过地址，需要按实际地址发送。
- `10`：写多个寄存器。
- `FF FD`：进入 IAP 地址。
- `00 01`：写 1 个寄存器。
- `02`：数据字节数。
- `00 01`：数据值。当前 App 不检查具体值，只检查数量是 1。
- `7D 72`：Modbus CRC，低字节在前。

如果使用旧串口上位机，它发送的连接命令同样是：

```text
function = 0x10
addr     = 0xFFFD
count    = 1
```

因此从“进入命令帧格式”看，旧协议和当前项目是兼容的。

### 2.2 App 写寄存器权限必须打开

当前工程配置：

```c
#define PROJECT_CFG_HOST_WRITE_ENABLE 1
```

如果这个宏关闭，`0x10/0xFFFD` 会在 App 层被拒绝，`AppUpgrade_RequestIap()` 不会被调用。

### 2.3 App 收到命令后不会马上在接收中断里复位

当前 App 逻辑是：

1. `Sci_WrRegs_0x10_FlashConnect()` 调用 `AppUpgrade_RequestIap()`。
2. 请求成功后置位 `u8FlashUpdateE2PROM = 1`。
3. 串口 ACK 发送完成后置位 `u8FlashUpdateFlag = 1`。
4. 主循环中的 `App_FlashUpdate()` 关闭 MOS，延时约 10 ms，然后 `MCU_RESET()`。

这个设计避免还没发完 ACK 就复位。

### 2.4 当前 App 写入的是 SRAM mailbox，不是旧 Flash flag

当前 `AppUpgrade_RequestIap()` 写入：

```text
0x20004FE0: magic
0x20004FE4: ~magic
0x20004FE8: request
0x20004FEC: ~request
0x20004FF0: crc
```

这个 mailbox 是复位后 IAP 判断“是否停留在 IAP”的入口条件。

`FLASH_ADDR_UPDATE_FLAG = 0x0801F800` 和 `FLASH_TO_IAP_VALUE = 0x00AB` 在当前 App 里保留为历史宏，但源码注释已经说明它们不是当前 boot gate。

## 3. 当前参考 IAP 的入口判断

参考 IAP：`E:\work\a002\new 030\IAP 103CB`

当前参考 IAP 启动后先执行：

```text
BootCtrl_ShouldJumpToApp()
```

判断逻辑：

1. 如果 SRAM mailbox 有效，消费该请求，停留在 IAP。
2. 如果没有有效 mailbox，且 App 向量有效，则跳转到 App。
3. 如果没有有效 App，留在 IAP。

因此，当前 App 和当前参考 IAP 是匹配的：二者都使用 `0x20004FE0` SRAM mailbox。

风险点在于：旧板实际烧录的 IAP 不一定是这个参考 IAP。如果旧板 IAP 是早期版本，只认 `0x0801F800 = 0x00AB`，那么当前 App 写 SRAM mailbox 对它无效。

## 4. IAP 阶段升级协议

IAP 真正运行后，串口协议仍然是旧升级协议风格。

### 4.1 IAP 连接

```text
slave           = 0x01
function        = 0x10
start address   = 0xFFFD
register count  = 1
byte count      = 2
payload         = 任意，当前新上位机用 00 00
```

典型帧：

```text
01 10 FF FD 00 01 02 00 00 BC B2
```

典型 ACK：

```text
01 10 FF FD 00 01 A0 2D
```

参考 IAP 中 `IapUpgrade_SerialConnect()` 只检查 `value_count == 1`，不检查 payload 值。

### 4.2 IAP 数据块写入

```text
slave           = 0x01
function        = 0x10
start address   = 0xFFFE
declared length = 数据长度或寄存器数量，取决于 byte_count
byte count      = 0 或标准 Modbus 字节数
payload         = bin 数据块
```

当前参考 IAP 支持两种长度解释：

1. `byte_count != 0`：标准 Modbus 写多寄存器，要求 `declared_length * 2 == byte_count`。
2. `byte_count == 0`：兼容旧 IAP 风格，`declared_length` 直接表示原始 payload 字节数。

当前新上位机直连串口 IAP 使用 `byte_count = 0` 的旧兼容方式，每块最大 `1024` 字节。

### 4.3 IAP 完成

```text
slave           = 0x01
function        = 0x10
start address   = 0xFFFF
register count  = 1
byte count      = 2
payload         = 00 00
```

典型帧：

```text
01 10 FF FF 00 01 02 00 00 BD 50
```

典型 ACK：

```text
01 10 FF FF 00 01 01 ED
```

## 5. 当前新上位机直连串口模式的实际链路

当前新上位机的“BMS 直连串口”模式，实际是：

```text
PC 上位机
  ↓ 串口，固定 115200
comm tool 串口1
  ↓ comm tool USART2，波特率由上位机选择
BMS App / BMS IAP
```

它不是裸 PC 串口直接接到 BMS。comm tool 负责把 PC 的 Modbus 帧透明转发到 USART2。

关键点：

- PC 到 comm tool 串口1固定 `115200`。
- comm tool USART2 到 BMS 的波特率由上位机波特率选择框设置。
- 当前 UI 里的波特率选择框应该理解为 **BMS 侧/comm tool USART2 波特率**。
- 如果 BMS App 是 `115200`，但 IAP 是 `19200`，进入 IAP 后必须切换 BMS 侧波特率，否则无法收到 IAP ACK。

## 6. 当前项目与旧板可能不兼容的点

### 6.1 入口标志不兼容

这是最高概率问题。

| 项目 | 当前 App/当前参考 IAP | 旧 IAP 可能机制 |
|---|---|---|
| 入口标志位置 | SRAM `0x20004FE0` | Flash `0x0801F800` |
| 入口标志值 | `magic/request/crc` | `0x00AB` |
| 标志生命周期 | 复位后由 IAP 消费并清除 | 可能由 IAP 或升级完成后写回 `0xFFFF` |
| 故障表现 | App ACK 后复位，IAP 停留 | App ACK 后复位，但旧 IAP 看不到标志，直接跳 App |

### 6.2 App 与 IAP 波特率不一致

当前源码观察到：

| 阶段 | 参考波特率 |
|---|---:|
| 当前 BMS App 串口 | `115200` |
| 参考 IAP 串口 | `19200` |
| comm tool 串口1/PC | `115200` |
| comm tool USART2/BMS | 由上位机选择，默认 `19200` |

如果 App 入口阶段要用 `115200`，但 IAP 升级阶段要用 `19200`，上位机流程必须支持“进入 IAP 后切换 BMS 侧波特率”。

旧串口上位机默认串口是 `19200`，并且收到 `0xFFFD` ACK 后等待约 5 秒再允许开始写入。新上位机当前等待更短，并且会主动再次发送 IAP connect。对当前 IAP 这是合理的，但对旧板的时序需要实测确认。

### 6.3 IAP 监听 UART 或硬件引脚不一致

当前 comm tool 透明转发到 BMS 的 USART2：

```text
TX = PA2
RX = PA3
```

如果旧板 IAP 实际监听的是另一个 UART 或另一组引脚，则 App 阶段可能能收到命令，但复位进 IAP 后收不到升级命令。

### 6.4 IAP 固定地址与 App 地址不一致

参考 IAP 固定：

```text
RS485_SLAVE_ADDR = 0x01
```

进入 IAP 之前，App 阶段要发给 App 当前地址；进入 IAP 后，要发给 IAP 固定地址 `0x01`。

如果旧板 App 地址被改过，而上位机仍按 `0x01` 发进入命令，App 根本不会进入 IAP。

## 7. 现象与原因对照

| 现象 | 优先怀疑 |
|---|---|
| 发送 `0xFFFD` 没有 App ACK | App 地址错误、App 波特率错误、CRC/功能码错误、`PROJECT_CFG_HOST_WRITE_ENABLE` 关闭、comm tool 透明桥未转发 |
| 有 App ACK，板子复位，但很快又能读到 App 数据 | 入口标志不兼容：当前 App 写 SRAM mailbox，旧 IAP 只认 Flash flag |
| 有 App ACK，板子复位后完全无响应 | IAP 波特率不一致、IAP UART/引脚不一致、IAP 异常、App 向量/跳转异常 |
| IAP connect 有 ACK，但写数据块失败 | 数据块长度解释不一致、单块超过 1024 字节、Flash 擦写失败、App 镜像超过 `0x0801F800` 限制 |
| 旧上位机可升级，新上位机不行 | 新上位机的 IAP 阶段等待时间、BMS 侧波特率切换、comm tool 透明桥状态机需要调整 |
| 新参考 IAP 可升级，旧板不可升级 | 旧板 IAP 入口标志、UART、波特率或 App 起始地址与当前项目不同 |

## 8. 建议的实测步骤

### 8.1 先区分“App 未收到命令”还是“已复位但 IAP 不接管”

用逻辑分析仪或串口抓包确认：

1. PC/comm tool 发出 `0x10/0xFFFD/count=1`。
2. BMS App 是否返回 ACK：

```text
01 10 FF FD 00 01 A0 2D
```

3. ACK 后 BMS 是否发生复位。
4. 复位后是回到 App，还是留在 IAP。

判断方法：

- 复位后还能正常读实时数据：大概率 IAP 没有识别入口标志，跳回 App。
- 复位后不能读实时数据，但 IAP connect 无 ACK：优先查 IAP 波特率/UART/引脚。

### 8.2 分别测试 App 波特率和 IAP 波特率

建议按以下组合测试：

| 阶段 | BMS 侧波特率 |
|---|---:|
| App 入口命令 | `115200` |
| IAP connect | `19200` |
| IAP connect | `115200` |

如果 `115200` 能让 App ACK，但只有 `19200` 能让 IAP ACK，则说明必须在上位机流程里区分 App 波特率和 IAP 波特率。

### 8.3 确认旧板 IAP 入口机制

需要确认旧板实际 IAP 代码或旧板 IAP binary 对应版本：

- 是否读取 `0x20004FE0` SRAM mailbox。
- 是否读取 `0x0801F800` Flash halfword。
- 如果读取 Flash flag，是否要求值为 `0x00AB`。
- 进入 IAP 后是否会清除该 flag。
- 升级完成后是否会写回 `0xFFFF`。

这是决定后续方案的关键。

## 9. 可选兼容方案

### 方案 A：不改当前 App，只更新旧板 IAP

把旧板 IAP 更新为当前 mailbox 机制。

优点：

- 当前 App 不需要增加旧兼容逻辑。
- App/IAP 入口机制统一。

缺点：

- 需要改写 Bootloader 区 `0x08000000`，现场风险较高。
- 如果旧板已经无法通过现有方式进入 IAP，需要借助 ST-Link 或生产烧录工具。

适用场景：

- 可以返厂或有可靠烧录夹具。
- 旧板数量少。
- 允许触碰 Bootloader 区。

### 方案 B：App 进入 IAP 时同时写 SRAM mailbox 和旧 Flash flag

在当前 App 的 `AppUpgrade_RequestIap()` 中保留 SRAM mailbox，同时可选写：

```text
0x0801F800 = 0x00AB
```

优点：

- 同一个 App 可以兼容当前 IAP 和旧 IAP。
- 对旧板现场升级最友好。

缺点/风险：

- 会擦写 `0x0801F800` 所在 Flash 页，必须确认该页没有其它有效存储数据。
- 需要确认旧 IAP 或 App 后续会把 flag 清回 `0xFFFF`，否则可能导致反复进入 IAP。
- 这是对启动边界的变更，必须做实板验证。

建议实现方式：

- 增加编译开关，例如：

```c
#define PROJECT_CFG_IAP_LEGACY_FLASH_FLAG_ENABLE 1
```

- 默认是否打开，需要根据旧板兼容范围决定。
- 只在收到 `0xFFFD` 进入 IAP 请求时写旧 flag，不要在普通启动流程中写。
- 写入失败时要返回 Modbus 异常，不能假装进入成功。

### 方案 C：新上位机增加“旧板 IAP 模式”

旧板模式建议包含：

1. App 阶段按用户选择的 App/BMS 波特率发送 `0xFFFD`。
2. 收到 App ACK 或检测到串口断连后，等待 5~8 秒。
3. 把 comm tool USART2/BMS 侧波特率切换到旧 IAP 波特率，默认 `19200`。
4. 用 IAP 固定地址 `0x01` 发送 `0xFFFD` connect。
5. 如果仍失败，允许尝试 `115200` 或用户选择的 IAP 波特率。

优点：

- 不触碰 BMS App 和 IAP。
- 能解决波特率、等待时间、旧上位机时序差异问题。

缺点：

- 不能解决入口标志不兼容。如果旧 IAP 只认 Flash flag，而当前 App 只写 SRAM mailbox，单靠上位机无法让旧 IAP 停留。

适用场景：

- 已确认旧板 IAP 能识别当前 SRAM mailbox，只是波特率/时序不匹配。
- 或旧板 App/IAP 本身是旧成套固件，只是新上位机流程没模拟旧上位机时序。

### 方案 D：当前 IAP 兼容读取 SRAM mailbox 和旧 Flash flag

在 IAP 启动判断中同时支持：

1. SRAM mailbox。
2. Flash flag `0x0801F800 = 0x00AB`。

优点：

- 新旧 App 都可以进入当前 IAP。
- 入口兼容性最好。

缺点：

- 需要更新 IAP，本质仍然触碰 Bootloader 区。
- 必须严格处理 Flash flag 清除，避免反复停留在 IAP。

适用场景：

- 后续新出厂板子可以统一烧录新版 IAP。
- 希望未来同时兼容老 App 和新 App。

## 10. 推荐决策路径

建议不要先盲改。按以下顺序决策：

1. **先确认旧板实际 IAP 版本和入口机制。**
   - 如果旧 IAP 只认 `0x0801F800 = 0x00AB`，优先考虑方案 B。
   - 如果旧 IAP 已经认 `0x20004FE0` mailbox，优先考虑方案 C。

2. **确认旧板 IAP 串口波特率和 UART。**
   - 如果 App 是 `115200`、IAP 是 `19200`，上位机必须支持 App 波特率和 IAP 波特率分离。
   - 如果旧板 IAP 不在 USART2 PA2/PA3 上，comm tool 透明桥方案无法直接覆盖，需要硬件接线或 IAP 适配。

3. **如果目标是现场兼容旧板升级，推荐组合：**

```text
方案 B：App 双入口标志
方案 C：上位机旧板 IAP 模式，支持 IAP 阶段波特率切换和更长等待
```

4. **如果目标是长期统一维护，推荐组合：**

```text
方案 D：新版 IAP 同时兼容 SRAM mailbox 和 Flash flag
方案 B：App 保留一段过渡期的旧 Flash flag 写入能力
后续确认旧板全部升级后，再关闭旧 Flash flag 兼容开关
```

## 11. 后续如果决定修改，需要重点验证

### 11.1 App 双入口标志验证

- 当前 IAP：仍能通过 SRAM mailbox 正常进入。
- 旧 IAP：能通过 Flash flag 正常进入。
- `0x0801F800` 页擦写不会破坏现有存储。
- 进入 IAP 失败时 App 返回异常，不返回成功。
- 升级完成后不会反复进入 IAP。

### 11.2 上位机旧板模式验证

- App 波特率 `115200` 可进入。
- 进入后切换 IAP 波特率 `19200` 可连接。
- 旧上位机能升级的板子，新上位机也能升级。
- 当前新 IAP 板子不受旧板模式影响。
- comm tool 透明桥在 BMS 复位期间不会卡死在等待旧响应。

### 11.3 安全边界

- IAP 地址仍固定为 `0x08000000`。
- App 地址仍固定为 `0x08004800`。
- App 镜像不能写到 `0x0801F800` 之后。
- 禁止把 App bin 裸写到 `0x08000000` 覆盖 IAP。

