# CAN 通信模块综合分析

> 源码文件：`Can_HDX.c` / `Can_HDX.h` / `CanFeidaoFrames.c` / `CanFeidaoFrames.h`

---

## 1. 模块概述：飞道（Feidao）CAN 协议

本模块是 BMS（电池管理系统）与外部上位机/从机之间的 CAN 总线通信层，采用**飞道协议**进行数据交互。协议核心特征：

| 特性 | 说明 |
|------|------|
| **总线标准** | CAN 2.0B，标准帧（11-bit StdId）用于命令交互，扩展帧（29-bit ExtId）用于周期广播 |
| **节点地址** | `CAN_ADRESS_STD_ID = 0x00`（定义在 `Can_HDX.h:5`） |
| **命令帧 ID** | `0x60`（CMD），`0x61`（ACK），均带地址偏移 `(addr << 7)` |
| **广播帧 ID** | 扩展帧基础 ID `0x14F80200`，低 8 位为通道号 |
| **帧格式** | 数据帧，DLC = 8，大端序（Big-Endian）编码 |
| **校验方式** | CRC-16 RTU，覆盖前 6 字节，结果存 Byte[6..7] |
| **收发架构** | 中断接收 + 主循环轮询发送，TX 队列 32 帧环形缓冲区 |

**模块职责划分：**
- `Can_HDX.c`：CAN 驱动核心（初始化、收发队列、中断处理、应用命令解析、低功耗）
- `CanFeidaoFrames.c`：飞道协议帧格式封装（周期广播帧的数据组装与调度）

---

## 2. 硬件配置

### 2.1 GPIO 配置（`InitCan_GPIO()`，Can_HDX.c:756-779）

| 引脚 | 方向 | 用途 | 速度 |
|------|------|------|------|
| `PB4`（`PIN_CMNT_EN`） | 推挽输出 | CAN 收发器供电控制 | 2 MHz |
| `PA11` | 上拉输入 | CAN1_RX | 2 MHz |
| `PA12` | 复用推挽输出 | CAN1_TX | 2 MHz |

- 开启 `AFIO`、`GPIOA`、`GPIOB` 时钟
- 禁用 JTAG（`GPIO_Remap_SWJ_JTAGDisable`），保留 SWD 调试口
- CAN 收发器上电控制：低电平有效（`FEIDAO_CAN_POWER_ON_LEVEL = Bit_RESET`）

### 2.2 NVIC 配置（`InitCan_NVIC()`，Can_HDX.c:781-790）

| 中断通道 | 抢占优先级 | 子优先级 | 使能 |
|----------|-----------|---------|------|
| `USB_LP_CAN1_RX0_IRQn` | 1 | 1 | 是 |

仅开启 RX FIFO0 接收中断，TX 完成不在中断中处理。

### 2.3 CAN 过滤器配置（`InitCan_Filter()`，Can_HDX.c:792-806）

| 参数 | 值 | 说明 |
|------|-----|------|
| FilterNumber | 0 | 使用过滤器组 0 |
| FilterMode | IdMask | 标识符屏蔽模式 |
| FilterScale | 32bit | 32 位宽 |
| FilterIdHigh/Low | 0x0000 / 0x0000 | 不过滤 |
| FilterMaskIdHigh/Low | 0x0000 / 0x0000 | 全部放行 |
| FIFOAssignment | FIFO0 | 接收 FIFO 0 |

**结论：过滤器全通，所有 CAN 帧均被接收。** 实际过滤在 `feidao_can_handle_rx_msg()` 中软件完成。

### 2.4 CAN1 控制器配置（`InitCan_CAN1()`，Can_HDX.c:808-828）

| 参数 | 值 | 说明 |
|------|-----|------|
| CAN_Mode | Normal | 正常模式 |
| CAN_ABOM | ENABLE | 自动离线恢复 |
| CAN_NART | ENABLE | 禁止自动重传（发送失败不重试） |
| CAN_TXFP | DISABLE | 发送 FIFO 优先级按请求顺序 |
| CAN_SJW | 1tq | 同步跳转宽度 |
| CAN_BS1 | 5tq | 位段 1 |
| CAN_BS2 | 2tq | 位段 2 |
| CAN_Prescaler | 4 | 分频系数 |

**波特率计算**（假设 APB1 = 36 MHz）：

```
CAN 时钟 = APB1 / Prescaler = 36 MHz / 4 = 9 MHz
tq = 1 / 9 MHz ≈ 111.1 ns
每 bit = 1 (SYNC) + 5 (BS1) + 2 (BS2) = 8 tq
波特率 = 9 MHz / 8 = 1.125 Mbps
```

> 注意：该波特率为 1.125 Mbps，非标准值（常见 500K/1000K）。需确认与上位机波特率匹配。

---

## 3. TX 队列管理（32 帧环形缓冲区）

### 3.1 数据结构（Can_HDX.c:59-74）

```c
typedef struct {
    CanTxMsg frame;     // CAN 帧
    UINT8 source;       // 来源标记
} FeidaoCanTxItem;

typedef struct {
    FeidaoCanTxItem queue[32];  // 环形缓冲区
    UINT8 head;                 // 读指针
    UINT8 tail;                 // 写指针
    UINT8 count;                // 当前帧数
    UINT8 mailbox;              // 正在发送的邮箱号
    UINT8 mailbox_source;       // 当前发送帧的来源
    UINT32 start_tick;          // 发送开始时间戳
} FeidaoCanTxRuntime;
```

### 3.2 帧来源分类

| 枚举值 | 含义 |
|--------|------|
| `FEIDAO_CAN_TX_SOURCE_PERIODIC` (0) | 周期广播帧 |
| `FEIDAO_CAN_TX_SOURCE_REQUEST` (1) | 请求/响应帧 |
| `FEIDAO_CAN_TX_SOURCE_NONE` (0xFF) | 空闲/无 |

### 3.3 入队 / 出队

- **入队** `feidao_can_enqueue_tx()`（第 207 行）：拷贝帧到 `queue[tail]`，tail 自增取模，count++
- **出队** `feidao_can_dequeue_tx()`（第 225 行）：拷贝 `queue[head]` 到调用方，head 自增取模，count--
- **清空** `feidao_can_clear_tx_queue()`（第 242 行）：head/tail/count 归零

### 3.4 发送调度（`feidao_can_service_tx()`，Can_HDX.c:271-319）

发送服务每次被 `App_Can()` 调用时执行，逻辑如下：

1. **检查当前邮箱**：若 `mailbox` 有效，轮询 `CAN_TransmitStatus()`：
   - `CAN_TxStatus_Ok` → 清除完成标志，释放邮箱
   - `CAN_TxStatus_Failed` → 清除失败标志，释放邮箱
   - 超时（200ms，即 `FEIDAO_CAN_TX_TIMEOUT_TICKS = 20` × 10ms 周期）→ 取消发送，释放邮箱
2. **获取新帧**：若邮箱空且队列非空，dequeue 一帧，调用 `CAN_Transmit()` 硬件发送
3. **记录调试信息**：成功入硬件邮箱后调用 `DBG_RecordCanTxFrame()`

### 3.5 优先级策略（`feidao_can_queue_has_request()`，Can_HDX.c:249-269）

遍历队列，若存在任何非 `PERIODIC` 来源的帧，返回 1。这用于低功耗判断：`can_has_sleep_blocking_work()` 只阻塞有请求帧的场景，周期帧可以在睡眠前丢弃。

### 3.6 地址偏移（`feidao_can_transmit()`，Can_HDX.c:321-338）

标准帧（StdId）在发送前会被加上地址偏移：

```c
frame.StdId += ((UINT32)CAN_ADRESS_STD_ID << 7);
// 即 StdId += 0x00 << 7 = 0x00（当前地址为 0，偏移为零）
```

这允许通过修改 `CAN_ADRESS_STD_ID` 实现多节点地址区分。

---

## 4. 关键函数分析

### 4.1 `InitCan()` — CAN 模块总初始化（Can_HDX.c:830-844）

```
行 830-844
```

执行顺序：
1. 初始化运行时状态：`s_tx.mailbox` = NoMailBox，`schedule_init` = 0，`enter_iap_delay_ticks` = 0，`read_block_active` = 0
2. 清空 TX 队列和应用命令队列
3. 调用 `InitCan_GPIO()` — GPIO 和收发器上电
4. 调用 `InitCan_NVIC()` — 中断使能
5. 调用 `InitCan_CAN1()` — 控制器配置
6. 调用 `InitCan_Filter()` — 过滤器配置
7. 再次调用 `feidao_can_power_on()` — 确保上电

**设计特点：** 分步骤、有序的初始化流程，幂等安全。

### 4.2 `App_Can()` — CAN 主循环入口（Can_HDX.c:925-939）

```
行 925-939
```

被主循环 10ms 周期调用，执行顺序：

| 步骤 | 函数 | 说明 |
|------|------|------|
| 1 | `SysTime_Get10msTickCount()` | 获取当前 10ms 系统计数 |
| 2 | `feidao_can_schedule_periodic()` | 调度周期广播帧入队 |
| 3 | `feidao_can_take_app_cmd()` + `handle_app_cmd_data()` | 处理一条待处理的命令 |
| 4 | `feidao_can_service_read_block_stream()` | 发送块读数据流 |
| 5 | `feidao_can_service_tx()` | 推进 TX 邮箱发送 |
| 6 | `feidao_can_service_enter_iap_delay()` | IAP 延时计数 |

**关键：** 每 10ms 只处理一条命令，避免长时间阻塞。

### 4.3 `feidao_can_service_tx()` — TX 发送服务（Can_HDX.c:271-319）

（已在第 3.4 节详述）

核心逻辑：
- 确保同一时刻只有一个邮箱在使用（`s_tx.mailbox` 单邮箱跟踪）
- 超时保护：200ms 未完成则取消
- `CAN_NART = ENABLE` 意味着硬件不会自动重传，超时后丢弃

### 4.4 `feidao_can_schedule_periodic()` — 周期广播调度（Can_HDX.c:369-388）

```
行 369-388
```

- 首次调用初始化时间戳基准（`schedule_init` 标志）
- **1000ms 周期**（每 100 个 tick）：入队 `CAN_FEIDAO_1000MS_MSG_MASK`（电压电流 + SOC）
- **5000ms 周期**（每 500 个 tick）：入队 `CAN_FEIDAO_5000MS_MSG_MASK`（容量 + SOH + 版本 + 状态 + 老化时间）

调度使用位掩码，`feidao_can_queue_periodic_mask()` 循环调用 `CanFeidao_SendNextPending()` 直到掩码清零或入队失败。

### 4.5 `feidao_can_handle_app_cmd_data()` — 应用命令处理（Can_HDX.c:559-725）

```
行 559-725
```

**帧格式校验：**
- Byte[0] = `0xA5`，Byte[1] = `0x5A`（魔数校验）
- Byte[6..7] = CRC-16 RTU 校验前 6 字节

**支持的命令（`data[2]` 字段）：**

| 命令码 | 名称 | 功能 | 参数 |
|--------|------|------|------|
| `0x01` | GET_STATUS | 读取 SOC/SOH | 无 |
| `0x02` | ENTER_IAP | 进入 IAP 升级 | Byte[3..5] = `0xC3, 0x3C, addr` |
| `0x03` | READ_REG | 读单个寄存器 | Byte[3..4] = 寄存器地址 |
| `0x04` | WRITE_PREP | 写准备（高位） | Byte[3..4] = 地址, Byte[5] = 高字节 |
| `0x05` | WRITE_COMMIT | 写提交（低位） | Byte[3..4] = 地址, Byte[5] = 低字节 |
| `0x06` | READ_BLOCK | 批量读 | Byte[3..4] = 起始地址, Byte[5] = 数量 |
| `0x07` | AGING_START | 启动老化 | Guard + 动作码 |
| `0x08` | AGING_STOP | 停止老化 | Guard + 动作码 |
| `0x09` | AGING_RESET_TIME | 重置老化时间 | Guard + 动作码 |
| `0x0A` | AGING_SET_HOURS | 设置老化时长 | Guard + 小时数 |

**回复帧格式：** ID = `0x61`（`FEIDAO_CAN_APP_ACK_ID`），Byte[0..1] = `0x5A, 0xA5`，Byte[2] = 命令码，Byte[3] = 状态码，Byte[4..5] = 数据。

**ACK 状态码：**

| 值 | 含义 |
|----|------|
| `0x00` | OK |
| `0x01` | BAD_CMD（未知命令） |
| `0x02` | BAD_PARAM（参数错误） |
| `0x05` | FLASH_ERR |
| `0x07` | NO_PERMISSION |
| `0x08` | BMS_ERROR |

**写命令采用两步提交（WRITE_PREP + WRITE_COMMIT）机制，** 防止单帧误写。

**批量读（READ_BLOCK）** 通过流式机制（`read_block_stream`）逐帧发送，每帧间隔 1 个 tick（10ms），最多 120 个 word。

### 4.6 `USB_LP_CAN1_RX0_IRQHandler()` — CAN 接收中断（Can_HDX.c:941-957）

```
行 941-957
```

中断处理流程：
1. 调用 `IrqDebug_CountFast()` 记录中断频率
2. `while` 循环遍历 FIFO0 中所有待读帧
3. 递增 `sys_time.can_rcv_cnt`（供主循环检测新消息）
4. `CAN_Receive()` 读取帧数据
5. `DBG_RecordCanRxFrame()` 记录调试信息
6. `feidao_can_handle_rx_msg()` 软件过滤并入队应用命令

**注意：** 中断中仅做数据拷贝和入队操作，不做任何耗时处理。应用命令在主循环中被取出执行。

---

## 5. 周期广播帧

### 5.1 帧 ID 规则

扩展帧 ID 格式：`0x14F802XX`，其中 `XX` 为通道号（`chd_index`）。

### 5.2 1000ms 周期帧

| 通道 | 函数 | 内容 |
|------|------|------|
| `0x00` | `CanFeidao_SendVoltageCurrent1000ms` | 电压（mV）+ 电流（mA，放电为负） |
| `0x02` | `CanFeidao_SendSoc1000ms` | 充电状态 + SOC + 最高温度(°C) + 预计充满时间 + 电池类型 + 保留 |

### 5.3 5000ms 周期帧

| 通道 | 函数 | 内容 |
|------|------|------|
| `0x01` | `CanFeidao_SendCap5000ms` | 实际容量（Wh）+ 设计容量（Wh） |
| `0x03` | `CanFeidao_SendSoh5000ms` | SOH + 循环次数 |
| `0x04` | `CanFeidao_SendVersion5000ms` | 协议版本 + 软件版本 |
| `0x05` | `CanFeidao_SendStatus5000ms` | 工作状态（MOS/充放）+ 异常码 + 额定/当前/设计容量 |
| `0x08` | `CanFeidao_SendFactoryTime5000ms` | 额定容量 + 老化状态 + 老化剩余分钟数 + 编译日期 |

### 5.4 状态码编码（`CanFeidao_SendStatus5000ms`）

**work_status（Byte[0]）：**

| Bit | 含义 |
|-----|------|
| 0 | 放电 MOS 开 |
| 1 | 充电 MOS 开 |
| 2 | 正在充电 |
| 3 | 充电中标志 |
| 4 | 正在放电 |

**exception_status（Byte[1]）：**

| 值 | 含义 |
|----|------|
| 0x02 | 放电过流 |
| 0x03 | 充电低温 |
| 0x04 | 充电高温 |
| 0x05 | 放电高温 |
| 0x06 | 欠压（单体/总压） |
| 0x07 | 过压（单体/总压） |
| 0x08 | 充电过流 |
| 0x09 | 放电低温 |
| 0x0C | CBC 放电保护 |
| 0x0D | 电压差过大 |

---

## 6. 应用命令协议（0x60 / 0x61）

### 6.1 接收路径

```
CAN 硬件 RX FIFO0
  → USB_LP_CAN1_RX0_IRQHandler() [中断]
    → feidao_can_handle_rx_msg() [过滤 StdId = (addr<<7)|0x60，DLC=8]
      → feidao_can_queue_app_cmd() [入队 cmd_queue，最多 4 条]
  → App_Can() [主循环 10ms]
    → feidao_can_take_app_cmd() [出队一条，CLI 保护]
      → feidao_can_handle_app_cmd_data() [解析并执行]
        → feidao_can_app_send_frame() [组装 ACK 并入队 TX]
```

### 6.2 发送路径

```
feidao_can_app_send_frame()
  → 构造 CanTxMsg（StdId = 0x61，DLC = 8）
  → 填充 0x5A 0xA5 + cmd + status + value0 + value1 + CRC
  → Can_HDX_Transmit() [source = REQUEST]
    → feidao_can_enqueue_tx()
  → App_Can() → feidao_can_service_tx() [最终发送]
```

### 6.3 命令队列的线程安全

- 命令队列 `cmd_queue` 使用 `volatile` + `__disable_irq()` / `__enable_irq()` 保护
- 中断中写入 `cmd_tail` / `cmd_count`（`feidao_can_queue_app_cmd`，注意：**此函数未加关中断**）
- 主循环中读取 `cmd_head` / `cmd_count`（`feidao_can_take_app_cmd`，已加关中断）

### 6.4 块读数据流（READ_BLOCK）

1. 收到 `0x06` 命令后，`Sci_HostReadWords()` 读取最多 120 个 word 到 `read_block_words[]`
2. `feidao_can_start_read_block_stream()` 启动流式发送
3. `feidao_can_service_read_block_stream()` 每 10ms 发送一帧（`0x86` 命令码），直到发完
4. 若 TX 队列接近满（`count > 28`），暂停发送等待队列排空

---

## 7. 低功耗管理

### 7.1 `Can_PrepareSleep()`（Can_HDX.c:917-923）

```c
void Can_PrepareSleep(void)
{
    feidao_can_abort_tx();           // 取消正在发送的帧，清空 TX 队列
    feidao_can_clear_app_cmd_queue(); // 清空待处理命令
    feidao_can_stop_read_block_stream(); // 停止块读流
    feidao_can_power_off();          // 关闭 CAN 收发器电源
}
```

### 7.2 `Can_IsBusy()` 与 `Can_PeekBusy()`（Can_HDX.c:894-915）

- `Can_PeekBusy()`：检查是否有任何待处理工作（包括周期帧），不做状态清除
- `Can_IsBusy()`：检查阻塞睡眠的工作（排除周期帧），同时消费 `can_rcv_cnt` 差值

**关键区别：** `Can_IsBusy()` 不会因周期帧而阻塞睡眠，只阻塞请求帧、块读流、命令队列等交互型工作。

### 7.3 睡眠阻塞判断逻辑（`can_has_sleep_blocking_work()`，Can_HDX.c:867-892）

满足以下任一条件则阻塞睡眠：
1. TX 队列中有请求帧（非周期帧）
2. 正在发送请求帧（非周期帧）
3. 块读数据流正在进行
4. 命令队列非空
5. 硬件邮箱状态异常（TSR_TME 不全置位）

---

## 8. 潜在问题分析

### 8.1 波特率非标准（Can_HDX.c:825）

```
Prescaler = 4, BS1 = 5tq, BS2 = 2tq → 1.125 Mbps
```

非标准波特率（常见 500K、1000K）。需确认上位机和所有节点均使用相同配置，否则通信失败。若 APB1 频率不是 36 MHz，实际波特率会有偏差。

### 8.2 发送过滤器全通（Can_HDX.c:799-802）

过滤器 Mask = 0x0000，所有帧均被接收。虽然在软件中通过 `feidao_can_handle_rx_msg()` 过滤 StdId，但所有 CAN 帧仍会触发 RX 中断，增加中断负载。在总线帧密集的场景下可能造成中断风暴。

**建议：** 配置硬件过滤器仅接收 `(addr<<7)|0x60` 的 StdId 帧，减少不必要的中断。

### 8.3 命令队列入队无关中断保护（Can_HDX.c:494-508）

`feidao_can_queue_app_cmd()` 由中断调用，写 `cmd_tail` 和 `cmd_count`，但未加 `__disable_irq()`。`feidao_can_take_app_cmd()` 由主循环调用，已加关中断。

实际上由于 `feidao_can_queue_app_cmd()` 只在中断上下文中被调用（中断嵌套不会发生），且主循环中的读取已用 CLI 保护，当前是安全的。但如果未来改为其他上下文调用需注意。

### 8.4 `exception_status` 覆盖问题（CanFeidaoFrames.c:201-240）

多个 `if` 语句按顺序赋值，高优先级故障可能被低优先级覆盖。例如同时存在 OCP（0x02）和 OVP（0x07），最终 `exception_status` 为 `0x07`（OVP），OCP 信息丢失。

**当前行为：** 后出现的故障覆盖前面的，异常码优先级由 `if` 顺序决定（OCP → 低温 → 高温 → 欠压 → 过压 → ...），越靠后的越优先。

### 8.5 版本号硬编码（CanFeidaoFrames.c:171-172）

```c
uint8_t pro_version = 1U;
uint16_t soft_version = 1U;
```

版本号为固定值 `1`，未从编译宏或配置中获取。每次版本更新需手动修改此处。

### 8.6 `CanFeidao_SendCap5000ms` 容量计算精度（CanFeidaoFrames.c:103-104）

```c
real_cap = CapacityNow * 10 * VCellTotle / 100;
design_cap = CapacityFactory * 10 * (36 * SNum) / 10;
```

乘法链中存在整数溢出风险。若 `CapacityFactory` 和 `SNum` 较大，`36 * SNum * 10` 可能溢出 `uint16`。建议使用 `uint32` 中间变量或调整计算顺序。

### 8.7 `CanFeidao_SendSoc1000ms` 温度转换（CanFeidaoFrames.c:140）

```c
temp = (int8_t)((int16_t)u16TempMax / 10 - 40);
```

先除 10 再减 40，若 `u16TempMax` 为 0，结果为 `-40`，范围合理。但整数除法精度有限，0.5 度分辨率丢失。

### 8.8 低功耗时收发器断电但中断未禁用

`Can_PrepareSleep()` 关闭收发器电源，但未禁用 CAN 中断。若在睡眠过渡期间有噪声干扰导致 RX 引脚电平变化，可能触发虚假中断。建议在关电源前先禁用 RX0 中断。

### 8.9 `CAN_NART` 模式（Can_HDX.c:818）

启用 `No Automatic Retransmission`，发送失败（如总线仲裁丢失或 ACK 缺失）不会重试。这对于实时性要求高的 BMS 数据是合理的，但上位机需自行处理丢帧。

---

## 附录：文件清单与行数

| 文件 | 行数 | 说明 |
|------|------|------|
| `Can_HDX.c` | 970 | CAN 驱动核心 |
| `Can_HDX.h` | 23 | 对外接口声明 |
| `CanFeidaoFrames.c` | 288 | 飞道帧格式封装 |
| `CanFeidaoFrames.h` | 26 | 帧掩码定义与接口 |

