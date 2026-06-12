# Comm Tool 与 CAN 上位机完整文档

---

## 一、系统架构总览

```
┌─────────────────────────────────────────────────────────────────┐
│                        PC 上位机                                 │
│  ┌──────────────────────┐  ┌──────────────────────┐             │
│  │ comm_tool_host.py    │  │ can_bms_host.py      │             │
│  │ (串口→Comm Tool)     │  │ (USB-CAN→BMS 直连)   │             │
│  │ 767 行               │  │ 675 行               │             │
│  └──────────┬───────────┘  └──────────┬───────────┘             │
│             │ UART 19200              │ USB-CAN                  │
└─────────────┼──────────────────────────┼────────────────────────┘
              │                          │
┌─────────────┼──────────────────────────┼────────────────────────┐
│             ▼                          ▼                        │
│  ┌──────────────────────┐  ┌──────────────────────┐            │
│  │ Comm Tool 固件       │  │ BMS App 固件          │            │
│  │ (STM32F103RET6)      │  │ (STM32F103C8)        │            │
│  │                      │  │                      │            │
│  │ PC ←UART→ MCU ←CAN→ │  │  ←CAN→ BMS MCU      │            │
│  │                      │  │                      │            │
│  │ 功能:                │  │ 功能:                │            │
│  │ - BMS 寄存器读写     │  │ - 保护/SOC/通讯     │            │
│  │ - CAN-IAP 升级       │  │ - CAN 协议处理       │            │
│  │ - 自身升级            │  │ - 参数存储           │            │
│  │ - 老化控制            │  │                      │            │
│  └──────────────────────┘  └──────────────────────┘            │
└─────────────────────────────────────────────────────────────────┘
```

### 1.1 两种连接方式对比

| 方式 | 路径 | 优点 | 缺点 |
|------|------|------|------|
| 串口→Comm Tool→CAN→BMS | PC → UART → F103RET6 → CAN → F103C8 | 支持升级、老化、诊断 | 多一跳，延迟较高 |
| USB-CAN→BMS 直连 | PC → USB-CAN → F103C8 | 延迟低，简单 | 不支持 Comm Tool 特有功能 |

---

## 二、Comm Tool 固件（STM32F103RET6）

### 2.1 文件结构

```
firmware/comm_tool_f103ret6/source/
├─ main.c                  (31 行)  — 入口，主循环
├─ stm32f10x_it.c         — 中断处理
├─ bsp/
│   ├─ board.c             — 系统初始化、GPIO 配置
│   ├─ board_uart.c        — UART 驱动（收发）
│   └─ board_can.c         (305 行) — CAN 驱动（初始化/收发/诊断）
├─ app/
│   ├─ ct_app.c            (548 行) — 命令处理主逻辑
│   ├─ ct_can_gateway.c    (670 行) — CAN 网关/协议转换
│   ├─ ct_upgrade_manager.c (454 行) — CAN-IAP 升级管理
│   ├─ ct_protocol.c       (164 行) — 串口协议解析
│   ├─ ct_flash_store.c    — Flash 存储（固件缓存）
│   ├─ ct_crc16.c          — CRC16 校验
│   ├─ ct_boot_control.c   — 启动控制
│   ├─ ct_debug_log.c      — 调试日志
│   └─ ct_self_iap.c       — 自身 IAP 升级
└─ iap/
    ├─ ct_iap_main.c       — IAP 入口
    └─ ct_iap.c            — IAP 实现
```

### 2.2 硬件配置

| 资源 | 分配 | 说明 |
|------|------|------|
| MCU | STM32F103RET6 | 512KB Flash, 64KB RAM |
| UART | USART1 (PB6/PB7) | 19200bps, 8N1, 与 PC 通讯 |
| CAN | CAN1 (PA11/PA12) | 250kbps (可配置), 与 BMS 通讯 |
| CAN 电源 | PB4 (CMNT_EN) | 低电平使能 CAN 收发器 |
| LED | PB5 | 运行指示灯 |
| 按键 | PA0 | 功能按键 |

### 2.3 主循环

```c
// main.c — 极简入口
int main(void)
{
    Board_Init();                    // 硬件初始化
    CtProtocol_Init(&s_parser);      // 协议解析器初始化
    CtApp_Init();                    // 应用初始化

    while (1)
    {
        while (BoardUart_ReadByte(&byte))  // 接收串口字节
        {
            CtSelfIap_FeedUartByte(byte);   // 喂给自身 IAP 检查
            if (CtProtocol_Feed(&s_parser, byte, &s_frame) != 0u)
            {
                CtApp_HandleFrame(&s_frame);  // 处理完整帧
            }
        }
        CtApp_Poll();       // 应用轮询（升级任务）
        Board_Poll();       // 板级轮询
    }
}
```

---

## 三、Comm Tool 串口协议

### 3.1 帧格式

```
请求帧:
[0x55][0xAA][VER][FLAGS][SEQ_H][SEQ_L][CMD][STATUS][LEN_H][LEN_L][PAYLOAD...][CRC16_H][CRC16_L]
  帧头     版本  标志   序号(16bit)  命令  状态    载荷长度(16bit)  载荷     CRC16(头+载荷)

应答帧:
[0x55][0xAA][VER][FLAGS|0x01][SEQ_H][SEQ_L][CMD][STATUS][LEN_H][LEN_L][PAYLOAD...][CRC16_H][CRC16_L]
                                     ↑ ACK 标志位置位
```

### 3.2 协议参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 帧头 | 0x55 0xAA | 固定同步字 |
| 版本 | 1 | 协议版本号 |
| 最大载荷 | 512 字节 | CT_UART_MAX_PAYLOAD |
| CRC 算法 | Modbus CRC16 | 初始值 0xFFFF, 多项式 0xA001 |
| 字节序 | 小端 | 载荷内数据 |

### 3.3 命令列表

| CMD | 名称 | 功能 | 最大载荷 |
|-----|------|------|---------|
| 0x01 | GET_INFO | 获取 Comm Tool 信息 | 24 字节 |
| 0x02 | SET_CAN | 设置 CAN 参数 | 6 字节 |
| 0x10 | BMS_READ | 读 BMS 寄存器 | 240 字节 |
| 0x11 | BMS_WRITE | 写 BMS 寄存器 | 240 字节 |
| 0x12 | BMS_AGING_CTRL | 老化控制 | 1 字节 |
| 0x13 | BMS_AGING_STATUS | 读老化状态 | 可变 |
| 0x14 | BMS_AGING_SET_HOURS | 设置老化时长 | 2 字节 |
| 0x20 | FW_BEGIN | 固件升级开始 | 14 字节 |
| 0x21 | FW_DATA | 固件数据写入 | 496 字节 |
| 0x22 | FW_END | 固件升级结束 | 10 字节 |
| 0x23 | FW_INFO | 读取固件信息 | 15 字节 |
| 0x30 | ENTER_IAP | 进入 IAP 模式 | 0 字节 |
| 0x31 | UPGRADE | 执行升级 | 0 字节 |
| 0x32 | UPGRADE_STATUS | 读取升级状态 | 可变 |
| 0x33 | UPGRADE_ABORT | 取消升级 | 0 字节 |
| 0x40 | RAW_CAN_TX | 原始 CAN 发送 | 13 字节 |
| 0x41 | CAN_DIAG | CAN 诊断信息 | 可变 |
| 0x42 | DEBUG_LOG | 读取调试日志 | 可变 |

### 3.4 GET_INFO 应答格式

```c
payload[0]  = CT_PROTOCOL_VERSION;   // 协议版本
payload[1]  = CT_FW_VERSION_MAJOR;   // 固件主版本
payload[2]  = CT_FW_VERSION_MINOR;   // 固件次版本
payload[3]  = CT_FW_VERSION_PATCH;   // 固件补丁版本
payload[4-7]  = can_bitrate;         // CAN 波特率 (32bit)
payload[8-11] = fw_cache_base;       // 固件缓存基地址
payload[12-15] = fw_cache_size;      // 固件缓存大小
payload[16-19] = debug_log_enabled;  // 调试日志使能
payload[20] = node_id;               // 节点 ID
payload[21] = app_can_addr;          // BMS CAN 地址
```

### 3.5 SET_CAN 命令格式

```c
payload[0-3] = can_bitrate;  // CAN 波特率 (32bit)
payload[4]   = node_id;      // 节点 ID (0x01~0x7F)
payload[5]   = app_can_addr; // BMS CAN 地址 (0x0~0xF)
```

---

## 四、Comm Tool CAN 网关

### 4.1 CAN 帧格式

```
BMS App 命令帧 (标准帧):
  ID = ((can_addr & 0x0F) << 7) | 0x060
  数据: [0xA5][0x5A][CMD][A0][A1][A2][CRC16_H][CRC16_L]

BMS App 应答帧 (标准帧):
  ID = ((can_addr & 0x0F) << 7) | 0x061
  数据: [0x5A][0xA5][CMD][STATUS][V0][V1][CRC16_H][CRC16_L]
```

### 4.2 核心函数

| 函数 | 行号 | 功能 |
|------|------|------|
| `CtCan_AppGetStatus` | 227 | 读取 BMS SOC/SOH |
| `CtCan_AppEnterIap` | 248 | 请求 BMS 进入 IAP |
| `CtCan_AppReadRegs` | 254 | 批量读 BMS 寄存器 |
| `CtCan_AppWriteRegs` | 351 | 批量写 BMS 寄存器 |
| `CtCan_AppAgingControl` | 420 | 老化控制 |
| `CtCan_AppSetAgingHours` | 445 | 设置老化时长 |
| `CtCan_ReadFactoryAgingBroadcast` | 460 | 读取老化广播帧 |
| `CtCan_IapSendHello` | 520 | IAP 握手 |
| `CtCan_IapSendStart` | 530 | IAP 开始传输 |
| `CtCan_IapSendData` | 540 | IAP 数据发送 |
| `CtCan_IapSendCommit` | 550 | IAP 块提交 |
| `CtCan_IapSendEnd` | 560 | IAP 结束 |

### 4.3 重试机制

```c
// send_app_cmd_wait_ack — 带重试的命令发送
static int send_app_cmd_wait_ack(uint8_t can_addr, uint8_t cmd, 
                                  uint8_t a0, uint8_t a1, uint8_t a2,
                                  uint8_t *v0, uint8_t *v1, uint32_t timeout_ms)
{
    start = CtBoard_GetTickMs();
    last_send = start - APP_CMD_RETRY_INTERVAL_MS;

    while (!timeout_expired(start, timeout_ms))
    {
        // 每 100ms 重发一次
        if ((CtBoard_GetTickMs() - last_send) >= APP_CMD_RETRY_INTERVAL_MS)
        {
            send_app_cmd(can_addr, cmd, a0, a1, a2);
            last_send = CtBoard_GetTickMs();
        }

        // 接收应答
        if (CtBoard_CanRecv(&frame, wait_ms))
        {
            match = decode_app_ack(&frame, can_addr, cmd, v0, v1);
            if (match == ACK_MATCH_OK) return 1;   // 成功
            if (match == ACK_MATCH_BAD) return 0;   // BMS 拒绝
        }
    }
    return 0;  // 超时
}
```

### 4.4 批量读取流程

```c
// CtCan_AppReadRegs — 批量读取 BMS 寄存器
int CtCan_AppReadRegs(uint8_t can_addr, uint16_t addr, uint16_t count, uint16_t *words)
{
    // 1. 尝试批量读取 (READ_BLOCK, 0x06)
    drain_can_rx();  // 清空接收队列
    if (!send_app_cmd_wait_ack(can_addr, CT_CAN_APP_READ_BLOCK, ...))
    {
        // 2. 回退：逐个读取 (READ_REG, 0x03)
        if (count > 4) return 0;  // 太多不回退
        for (i = 0; i < count; ++i)
        {
            send_app_cmd_wait_ack(can_addr, CT_CAN_APP_READ_REG, ...);
        }
        return 1;
    }

    // 3. 接收数据块帧
    while (!timeout && received_count < count)
    {
        CtBoard_CanRecv(&frame, wait_ms);
        decode_app_word_frame(&frame, ...);
        words[seq] = value;
    }

    return (received_count == count) ? 1 : 0;
}
```

---

## 五、Comm Tool 升级管理器

### 5.1 升级流程

```
Phase 0: IDLE
  │
  ▼
Phase 1: HELLO_FAST_SEND (700ms)
  │ 发送快速握手
  ▼
Phase 2: HELLO_FAST_WAIT
  │ 等待 BMS 应答
  ├─ 成功 → Phase 4
  └─ 失败 → Phase 3
  │
Phase 3: ENTER_APP_IAP
  │ 发送 ENTER_IAP 命令
  │ 等待 1200ms (Boot 启动)
  ▼
Phase 4: HELLO_IAP_SEND (8s)
  │ 发送 IAP 握手
  ▼
Phase 5: HELLO_IAP_WAIT
  │ 等待 IAP 应答
  ▼
Phase 6: START_SEND
  │ 发送 IAP_START (size, crc)
  ▼
Phase 7: START_WAIT
  │ 等待应答
  ▼
Phase 8: LOAD_BLOCK
  │ 从缓存加载 2KB 数据块
  ▼
Phase 9: SEND_DATA
  │ 发送 IAP_DATA × N
  ▼
Phase 10: SEND_COMMIT
  │ 发送 IAP_COMMIT
  ▼
Phase 11: COMMIT_WAIT
  │ 等待应答
  ▼
Phase 12: SEND_END
  │ 发送 IAP_END
  ▼
Phase 13: END_WAIT
  │ 等待最终应答
  ▼
Phase 14: DONE
```

### 5.2 关键超时参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 快速握手超时 | 700ms | Phase 1-2 |
| Boot 启动延迟 | 1200ms | Phase 3 |
| IAP 握手超时 | 8000ms | Phase 4-5 |
| 握手重试间隔 | 250ms | Phase 4 |
| 数据块大小 | 2KB | CT_IAP_BLOCK_BYTES |
| 命令重试间隔 | 100ms | 通用 |

---

## 六、CAN 上位机（Python）

### 6.1 can_bms_host.py

**功能**：直接通过 USB-CAN 适配器连接 BMS

```python
# 主要功能
├─ 监听 CAN 周期广播
│   ├─ 0x14F80200: 电压电流 (1s)
│   ├─ 0x14F80201: SOC (1s)
│   ├─ 0x14F80202: 容量 (5s)
│   ├─ 0x14F80203: SOH (5s)
│   ├─ 0x14F80204: 版本 (5s)
│   ├─ 0x14F80205: 状态 (5s)
│   └─ 0x14F80206: 老化时间 (5s)
│
├─ 发送应用命令
│   ├─ 0x01: GET_STATUS
│   ├─ 0x02: ENTER_IAP
│   ├─ 0x03: READ_REG
│   ├─ 0x04/0x05: WRITE_PREP/COMMIT
│   └─ 0x07~0x0A: AGING 控制
│
├─ CAN-IAP 升级
│   ├─ 固件分包 (2KB 块)
│   ├─ CRC16 校验
│   └─ 传输进度显示
│
└─ CAN 诊断
    ├─ 总线状态
    ├─ 错误计数
    └─ 寄存器快照
```

**广播帧解析示例**：

```python
def decode_feidao_broadcast(arbitration_id, data):
    ch = arbitration_id & 0xFF
    
    if ch == 0 and len(data) >= 8:  # 电压电流
        voltage_mv = be_u32(data, 0)
        current_ma = be_i32(data, 4)
        return f"总压={voltage_mv/1000:.3f}V 电流={current_ma/1000:.3f}A"
    
    if ch == 2 and len(data) >= 8:  # SOC
        status = data[0]
        soc = data[1]
        temp_c = signed_i8(data[2])
        charge_time = be_u16(data, 3)
        return f"充电状态={status} SOC={soc}% 温度={temp_c}C"
    
    if ch == 8 and len(data) >= 8:  # 老化时间
        factory_cap = be_u16(data, 0)
        aging_state = data[2]
        aging_remaining_min = be_u16(data, 3)
        return f"老化={aging_state_name(aging_state)} 剩余={format_remaining_minutes(aging_remaining_min)}"
```

### 6.2 comm_tool_host.py

**功能**：通过串口连接 Comm Tool

```python
# 主要功能
├─ BMS 寄存器读写（透传到 CAN）
│   ├─ CMD_BMS_READ (0x10)
│   └─ CMD_BMS_WRITE (0x11)
│
├─ Comm Tool 固件升级
│   ├─ CMD_FW_BEGIN (0x20)
│   ├─ CMD_FW_DATA (0x21)
│   ├─ CMD_FW_END (0x22)
│   └─ CMD_UPGRADE (0x31)
│
├─ BMS 固件升级（通过 Comm Tool）
│   └─ CMD_UPGRADE (0x31) → Comm Tool 转发到 BMS
│
├─ 老化控制
│   ├─ CMD_BMS_AGING_CTRL (0x12)
│   ├─ CMD_BMS_AGING_STATUS (0x13)
│   └─ CMD_BMS_AGING_SET_HOURS (0x14)
│
├─ CAN 诊断
│   └─ CMD_CAN_DIAG (0x41)
│
└─ 调试日志
    └─ CMD_DEBUG_LOG (0x42)
```

**帧收发流程**：

```python
class CommToolClient:
    def command(self, cmd, payload, timeout):
        # 1. 构建帧
        self._seq = (self._seq + 1) & 0xFFFF
        frame = encode_frame(self._seq, cmd, payload)
        
        # 2. 发送
        self._ser.write(frame)
        self._ser.flush()
        
        # 3. 等待应答（匹配 seq 和 cmd）
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            resp = read_frame(self._ser, timeout)
            if resp.seq != self._seq:
                continue  # 丢弃不匹配的帧
            if resp.cmd != cmd:
                continue
            if (resp.flags & FLAG_ACK) == 0:
                continue
            if resp.status != 0:
                raise RuntimeError(f"命令失败: {STATUS_TEXT[resp.status]}")
            return resp
        
        raise TimeoutError("等待响应超时")
```

---

## 七、数据流完整路径

### 7.1 BMS 寄存器读取

```
PC (comm_tool_host.py)
  │ CMD_BMS_READ (0x10, addr, count)
  ▼
Comm Tool 固件 (ct_app.c:handle_bms_read)
  │ CtCan_AppReadRegs(can_addr, addr, count, words)
  ▼
CAN 网关 (ct_can_gateway.c)
  │ 发送 READ_BLOCK (0x06) 命令
  │ 等待 ACK
  │ 接收数据块帧 (0x86)
  ▼
BMS App (Can_HDX.c)
  │ feidao_can_handle_app_cmd_data()
  │ → Sci_HostReadWords(addr, count, words)
  ▼
BMS App (Sci_Upper.c)
  │ 读取寄存器数据（g_stCellInfoReport / PRT_E2ROMParas / OtherElement）
  ▼
CAN 总线
  │ 扩展帧 0x86 (READ_BLOCK_DATA) × N
  ▼
Comm Tool (ct_can_gateway.c)
  │ 收集数据块，组装到 words[]
  ▼
串口
  │ 应答帧 (CMD_BMS_READ, words[])
  ▼
PC (comm_tool_host.py)
  │ 解析并显示
```

### 7.2 CAN-IAP 升级流程

```
PC (can_bms_host.py 或 comm_tool_host.py)
  │ 开始升级
  ▼
Comm Tool (ct_upgrade_manager.c)
  │ Phase 1: HELLO_FAST → BMS 快速握手
  │ Phase 2: ENTER_IAP → BMS 进入 IAP 模式
  │ Phase 3: 等待 Boot 启动 (1200ms)
  │ Phase 4: IAP_HELLO → IAP 握手
  │ Phase 5: IAP_START → 开始传输 (size, crc)
  │ Phase 6: IAP_DATA × N → 数据传输 (2KB 块)
  │ Phase 7: IAP_COMMIT → 块提交
  │ Phase 8: IAP_END → 结束
  ▼
CAN 总线
  │ IAP 控制帧 (0x14F8F000)
  │ IAP 数据帧 (0x14000000 + seq)
  ▼
BMS App IAP Bootloader
  │ 接收数据 → 写入 Flash → CRC 校验 → 跳转 App
```

### 7.3 广播帧监听

```
BMS App (Can_HDX.c)
  │ 周期广播
  ├─ 0x14F80200: 电压电流 (每秒)
  ├─ 0x14F80201: SOC (每秒)
  ├─ 0x14F80202: 容量 (每 5 秒)
  ├─ 0x14F80203: SOH (每 5 秒)
  ├─ 0x14F80204: 版本 (每 5 秒)
  ├─ 0x14F80205: 状态 (每 5 秒)
  └─ 0x14F80206: 老化时间 (每 5 秒)
  ▼
CAN 总线
  │ 扩展帧
  ▼
PC (can_bms_host.py)
  │ decode_feidao_broadcast()
  │ 解析并显示
```

---

## 八、关键配置参数

### 8.1 Comm Tool 配置

| 参数 | 默认值 | 位置 | 说明 |
|------|--------|------|------|
| CAN 波特率 | 250kbps | board_can.c | 可通过命令动态修改 |
| CAN 地址 | 0 | ct_app.c | 用于 App 命令 (0x0~0xF) |
| 节点 ID | 1 | ct_app.c | 用于 IAP 命令 |
| UART 波特率 | 19200 | board_uart.c | 固定 |
| 固件缓存基址 | 0x08008000 | ct_config.h | Comm Tool App 区域 |
| 固件缓存大小 | 128KB | ct_config.h | 可存储完整固件 |
| 最大载荷 | 512 字节 | ct_config.h | 串口帧最大载荷 |
| 命令重试间隔 | 100ms | ct_can_gateway.c | CAN 命令重试 |
| 快速握手超时 | 700ms | ct_upgrade_manager.c | Phase 1-2 |
| IAP 握手超时 | 8000ms | ct_upgrade_manager.c | Phase 4-5 |
| 数据块大小 | 2KB | ct_config.h | CAN-IAP 数据块 |

### 8.2 BMS CAN App 命令

| 命令 | ID | 功能 | 超时 | 参数 |
|------|-----|------|------|------|
| GET_STATUS | 0x01 | 读 SOC/SOH | 1s | 无 |
| ENTER_IAP | 0x02 | 进入 IAP | 5s | 0xC3, 0x3C, addr |
| READ_REG | 0x03 | 读单寄存器 | 1s | addr_h, addr_l |
| WRITE_PREP | 0x04 | 写准备 | — | addr_h, addr_l, val_h |
| WRITE_COMMIT | 0x05 | 写提交 | — | addr_h, addr_l, val_l |
| READ_BLOCK | 0x06 | 读数据块 | 6s | addr_h, addr_l, count |
| AGING_START | 0x07 | 启动老化 | 2s | guard, action, addr |
| AGING_STOP | 0x08 | 停止老化 | 2s | guard, action, addr |
| AGING_RESET | 0x09 | 重置时间 | 2s | guard, action, addr |
| AGING_SET_HOURS | 0x0A | 设置时长 | 2s | guard, hours, addr |

### 8.3 PC 工具配置

| 工具 | 依赖 | 用途 |
|------|------|------|
| can_bms_host.py | python-can | USB-CAN 直连 BMS |
| comm_tool_host.py | pyserial | 串口连接 Comm Tool |
| comm_tool_upgrade_ui.py | pyserial, tkinter | 升级 GUI |
| soc_test_ui.py | pyserial, tkinter | SOC 测试 GUI |

---

## 九、潜在问题

### 9.1 Comm Tool 固件问题

| 问题 | 严重度 | 说明 |
|------|--------|------|
| 无看门狗 | 中 | 主循环无 IWDG，死机无法自动恢复 |
| CAN 波特率非标准 | 低 | 1.125Mbps 非标准，需确认硬件匹配 |
| 升级超时硬编码 | 低 | 各阶段超时写死，无法适配不同网络延迟 |
| 单寄存器回退效率低 | 低 | 批量读失败时逐个读，120 个寄存器需 120 次 CAN 交互 |

### 9.2 PC 工具问题

| 问题 | 严重度 | 说明 |
|------|--------|------|
| 无自动重连 | 低 | 串口超时直接报错，无重试机制 |
| 无进度条 | 低 | 升级过程无实时进度显示 |
| 线程阻塞 | 低 | 同步收发，长命令阻塞 UI |

### 9.3 协议问题

| 问题 | 严重度 | 说明 |
|------|--------|------|
| 无加密 | 中 | 升级固件明文传输 |
| 无身份认证 | 中 | 任何设备可发送命令 |
| 序列号仅 16 位 | 低 | 理论上可重复 |

---

## 十、使用指南

### 10.1 启动 Comm Tool 通讯

```powershell
# PowerShell
.\tools\start_comm_tool_host.ps1

# 或直接运行 Python
python tools/comm_tool_host.py --port COM4 --baud 19200
```

### 10.2 启动 CAN 直连

```powershell
# PowerShell
.\tools\start_can_bms_host.ps1

# 或直接运行 Python
python tools/can_bms_host.py --interface pcan --channel PCAN_USBBUS1 --bitrate 250000
```

### 10.3 常用命令示例

```python
# 读取 BMS SOC
client = CommToolClient("COM4", 19200, 5.0)
resp = client.command(CMD_BMS_READ, struct.pack("<HH", 0xD000, 2))
soc = struct.unpack_from("<H", resp.payload, 20)[0]  # SOC 在偏移 20

# 通过 CAN 直连读取
bus = can.interface.Bus(bustype='pcan', channel='PCAN_USBBUS1', bitrate=250000)
# 发送 GET_STATUS 命令
frame = can.Message(arbitration_id=0x60, data=[0xA5, 0x5A, 0x01, 0, 0, 0, crc_h, crc_l])
bus.send(frame)
```

---

## 十一、总结

### 11.1 架构评价

```
Comm Tool 固件:
├─ 优点: 协议清晰、升级可靠、调试日志完善
├─ 缺点: 无看门狗、超时硬编码、无加密
└─ 评分: ★★★★☆

PC 上位机:
├─ 优点: 功能完整、协议实现正确、错误处理完善
├─ 缺点: 无自动重连、无进度条、线程阻塞
└─ 评分: ★★★☆☆

整体架构:
├─ 优点: 职责清晰、扩展性好、支持多节点
├─ 缺点: 多一跳延迟、无身份认证
└─ 评分: ★★★★☆
```

### 11.2 改进建议

| 优先级 | 建议 | 收益 |
|--------|------|------|
| 高 | Comm Tool 增加 IWDG | 防止死机 |
| 中 | PC 工具增加自动重连 | 用户体验 |
| 中 | 升级固件加密 | 安全性 |
| 低 | 协议增加身份认证 | 安全性 |
