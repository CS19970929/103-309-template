# BMS 串口通信（Modbus RTU）模块分析

> 分析文件：`103 + 309/Project/Source/Sci_Upper.c`（2112 行）、`Sci_Upper.h`（494 行）
> 分析日期：2026-06-12

---

## 1. 模块概述

本模块实现了基于 **Modbus RTU** 协议的串口通信功能，作为 BMS 上位机与从机之间的数据交互通道。支持的 Modbus 功能码：

| 功能码 | 含义 | 说明 |
|--------|------|------|
| `0x03` | 读保持寄存器 | 主机读取 BMS 数据/参数 |
| `0x06` | 写单个寄存器 | 主机写入单个控制命令或参数 |
| `0x10` | 写多个寄存器 | 主机批量写入参数/校准系数等 |

协议参数：波特率 19200，8 数据位，1 停止位，无校验（见 `Sci_Upper.c:1598-1601`）。

从机地址固定为 `0x01`（`RS485_SLAVE_ADDR`），广播地址 `0x00`（`RS485_BROADCAST_ADDR`）。

模块最多支持 **3 个串口**（USART1/USART2/USART3），通过编译宏 `_COMMOM_UPPER_SCI2`、`_COMMOM_UPPER_SCI3` 控制是否编译。所有串口共享同一套 Modbus 协议处理逻辑，仅端口运行时数据不同。

---

## 2. 架构分层

模块采用三层架构设计：

### 2.1 硬件抽象层（Port 层）

负责 USART 硬件初始化、中断处理、收发控制。

**核心数据结构：**

```c
struct SCI_PORT_RUNTIME {           // Sci_Upper.c:83-95
    USART_TypeDef *pstUsart;        // USART 外设指针
    void *pvProtocolCtx;            // 指向协议上下文（RS485MSG）
    const struct SCI_PROTOCOL_OPS *pstProtocolOps;  // 协议操作接口
    volatile UINT16 *pu16ErrorCounter;  // 错误计数器
    volatile UINT8 *pu8TxEnableFlag;    // 发送使能标志
    volatile UINT8 *pu8TxFinishFlag;    // 发送完成标志
    UINT8 *pu8TxBuffer;             // 发送缓冲区指针
    UINT16 u16TxIndex;              // 当前发送偏移
    UINT16 u16TxLength;             // 待发送总长度
    UINT8 u8FramePending;           // 有待处理帧标志
};
```

**三个端口实例：**

| 实例 | 定义位置 | USART | TX/RX 引脚 | 说明 |
|------|----------|-------|-----------|------|
| `g_stSciPort1` | Sci_Upper.c:138-148 | USART1 | PB6/PB7 (remap) | 默认串口 |
| `g_stSciPort2` | Sci_Upper.c:151-161 | USART2 | PA2/PA3 | 可选 |
| `g_stSciPort3` | Sci_Upper.c:165-175 | USART3 | PD8/PD9 (full remap) | 可选 |

### 2.2 协议层（Protocol 层）

负责 Modbus RTU 帧的解析、校验和应答组装。通过虚函数表实现与 Port 层解耦：

```c
struct SCI_PROTOCOL_OPS {           // Sci_Upper.c:71-81
    SCI_PROTOCOL_RESET_FN pfReset;          // 重置协议状态
    SCI_PROTOCOL_RX_FEED_FN pfRxFeed;       // 逐字节喂入接收数据
    SCI_PROTOCOL_PROCESS_FN pfProcessFrame;  // 处理完整帧
    SCI_PROTOCOL_TX_BUFFER_FN pfGetTxBuffer; // 获取发送缓冲区
    SCI_PROTOCOL_TX_LENGTH_FN pfGetTxLength; // 获取发送长度
    SCI_PROTOCOL_IS_BUSY_FN pfIsBusy;       // 忙碌状态查询
    SCI_PROTOCOL_RX_IDLE_FN pfOnRxIdle;     // 接收空闲回调
    SCI_PROTOCOL_TX_COMPLETE_FN pfOnTxComplete; // 发送完成回调
};
```

具体实现绑定在 `g_stSciModbusProtocolOps`（Sci_Upper.c:128-136）。

### 2.3 应用层（Application 层）

负责具体的寄存器读写业务逻辑，包括参数校验、Flash 存储、副作用处理。

---

## 3. 关键函数（含行号）

### 3.1 初始化

| 函数 | 位置 | 说明 |
|------|------|------|
| `InitUSART_CommonUpper` | Sci_Upper.c:2060-2073 | 总入口，按宏开关调用各串口初始化 |
| `InitSCI1_CommonUpper` | Sci_Upper.c:1670-1681 | USART1 初始化（PB6/PB7 remap） |
| `InitSCI2_CommonUpper` | Sci_Upper.c:1683-1696 | USART2 初始化（PA2/PA3） |
| `InitSCI3_CommonUpper` | Sci_Upper.c:1698-1711 | USART3 初始化（PD8/PD9 full remap） |
| `Sci_InitCommonPort` | Sci_Upper.c:1555-1635 | 通用端口初始化：时钟、GPIO、NVIC、USART 参数、中断使能 |
| `Sci_DataInit` | Sci_Upper.c:282-297 | 初始化 RS485MSG 结构和发送缓冲区 |

### 3.2 主循环调度

| 函数 | 位置 | 说明 |
|------|------|------|
| `App_CommonUpper` | Sci_Upper.c:2075-2088 | 主循环调用，逐端口轮询 `Sci_PortService` |
| `Sci_PortService` | Sci_Upper.c:1493-1533 | 检查 `u8FramePending`，调用协议处理→获取发送缓冲→启动发送或中止 |

### 3.3 中断处理（ISR 层）

| 函数 | 位置 | 说明 |
|------|------|------|
| `Sci1_CommonUpper_IRQHandler` | Sci_Upper.c:1651-1654 | USART1 中断入口 |
| `Sci2_CommonUpper_IRQHandler` | Sci_Upper.c:1656-1661 | USART2 中断入口 |
| `Sci3_CommonUpper_IRQHandler` | Sci_Upper.c:1663-1668 | USART3 中断入口 |
| `Sci_PortIRQHandler` | Sci_Upper.c:1402-1491 | **核心中断处理函数**，处理 RXNE/IDLE/TC/ERROR 四种中断 |
| `Sci_PortHandleError` | Sci_Upper.c:1387-1400 | 错误中断：读 DR 清标志、累加错误计数、中止传输 |
| `Sci_PortArmReceiver` | Sci_Upper.c:1305-1318 | 重新武装接收器：清 pending、使能 RXNE+IDLE 中断 |
| `Sci_PortStartTx` | Sci_Upper.c:1342-1357 | 切换到发送模式：关 RX、开 TXE 中断 |
| `Sci_PortFinishTx` | Sci_Upper.c:1359-1385 | 发送完成：清 TC、回调 OnTxComplete、触发 Flash 更新、重新武装接收 |
| `Sci_PortAbortTransfer` | Sci_Upper.c:1320-1340 | 中止传输：清标志、重置协议、重新武装接收 |

### 3.4 Modbus 协议处理

| 函数 | 位置 | 说明 |
|------|------|------|
| `Sci_ModbusProtocolFeed` | Sci_Upper.c:1135-1227 | **逐字节状态机**：地址校验→命令识别→帧长判断，返回 1 表示帧完整 |
| `Sci_ModbusProcessFrame` | Sci_Upper.c:1229-1271 | 帧处理主入口：CRC 校验→按命令分发→组装应答 |
| `CRC_verify` | Sci_Upper.c:299-316 | CRC16 校验：提取帧末 CRC 与计算值比对 |
| `Sci_ModbusResetMessage` | Sci_Upper.c:1113-1128 | 重置消息状态为 IDLE |
| `Sci_ModbusOnRxIdle` | Sci_Upper.c:1290-1298 | IDLE 中断回调：若收到不完整数据则丢弃 |
| `Sci_ModbusOnTxComplete` | Sci_Upper.c:1300-1303 | 发送完成回调：重置消息状态 |
| `Sci_ModbusIsBusy` | Sci_Upper.c:1283-1288 | 判断协议层是否忙碌 |
| `Sci_ModbusGetTxBuffer` | Sci_Upper.c:1273-1276 | 返回发送缓冲区指针 |
| `Sci_ModbusGetTxLength` | Sci_Upper.c:1278-1281 | 返回应答长度 |

### 3.5 0x03 读寄存器

| 函数 | 位置 | 说明 |
|------|------|------|
| `Sci_Deal_ReadRegs_0x03` | Sci_Upper.c:357-391 | 解析起始地址和寄存器数量，范围校验 |
| `Sci_FindRegion` | Sci_Upper.c:318-355 | 地址区域查找：RO 区→LCD 区→RW 区 |
| `Sci_RangeFits` | Sci_Upper.c:546-553 | 检查请求范围是否在区域内 |
| `Sci_ACK_0x03` | Sci_Upper.c:926-987 | 组装 0x03 应答帧（含 CRC） |
| `Sci_ACK_0x03_ReadRegs_Data` | Sci_Upper.c:772-841 | 读取实时数据区（0xD000 起，含电芯电压/温度/SOC/状态/故障） |
| `Sci_ACK_0x03_ReadRegs_LCD` | Sci_Upper.c:724-770 | 读取 LCD/事件记录/序列号区（0xC000 起） |
| `Sci_ACK_0x03_RW_Data_Pro` | Sci_Upper.c:855-865 | 读保护参数区 |
| `Sci_ACK_0x03_RW_Data_Cali` | Sci_Upper.c:867-879 | 读校准系数区（K/B 对） |
| `Sci_ACK_0x03_RW_Data_Other` | Sci_Upper.c:890-912 | 读 SOC 表 + RTC 区 |
| `Sci_ACK_0x03_RW_Data_OtherCanAdd` | Sci_Upper.c:914-924 | 读 OtherElement 区（均衡/休眠/SOC/系统参数） |

### 3.6 0x06 写单寄存器

| 函数 | 位置 | 说明 |
|------|------|------|
| `Sci_Deal_WrReg_0x06` | Sci_Upper.c:392-444 | 分发 0x06 命令到各处理函数 |
| `Sci_WrReg_0x06_Reset_ProtectRecord` | Sci_Upper.c:1904-1925 | 清除保护记录 |
| `Sci_WrReg_0x06_Reset_ProtectElement` | Sci_Upper.c:1927-1957 | 恢复保护参数默认值 |
| `Sci_WrReg_0x06_Reset_OtherCanAdd` | Sci_Upper.c:1959-1990 | 恢复 OtherElement 默认值 |
| `Sci_WrReg_0x06_BMS_FunctionON` | Sci_Upper.c:1995-2026 | 打开 BMS 功能（均衡/MOS/AFE/老化/休眠等） |
| `Sci_WrReg_0x06_BMS_FunctionOFF` | Sci_Upper.c:2028-2043 | 关闭 BMS 功能 |
| `Sci_WrReg_0x06_SetSocOnce` | Sci_Upper.c:2045-2058 | 设置 SOC 值（0-100） |

### 3.7 0x10 写多寄存器

| 函数 | 位置 | 说明 |
|------|------|------|
| `Sci_Deal_WrRegs_0x10` | Sci_Upper.c:691-722 | 分发 0x10 命令（先查 AFE→校准→dispatch 表） |
| `Sci_WrRegs_0x10_Protect` | Sci_Upper.c:1718-1758 | 写保护参数（含范围校验 + 快照回滚） |
| `Sci_WrRegs_0x10_OtherElement` | Sci_Upper.c:1770-1810 | 写 OtherElement（含范围校验 + 快照回滚 + 副作用） |
| `Sci_WrRegs_0x10_FlashConnect` | Sci_Upper.c:1837-1859 | 触发 IAP 升级连接 |
| `Sci_WrRegs_0x10_SN_Version` | Sci_Upper.c:1864-1898 | 写序列号/硬件版本/软件版本 |
| `Sci_WrRegs_0x10_CalibCoef` | Sci_Upper.c:1713-1715 | 校准系数写入（空实现，由外部提供） |
| `Sci_WrRegs_0x10_SocTable` | Sci_Upper.c:1760-1764 | SOC 表写入（拒绝写入，返回错误） |
| `Sci_WrRegs_0x10_RTC` | Sci_Upper.c:1766-1768 | RTC 写入（空实现） |

### 3.8 内部编程接口

| 函数 | 位置 | 说明 |
|------|------|------|
| `Sci_HostReadWords` | Sci_Upper.c:1014-1059 | 内部模拟主机读寄存器（用于模块间数据获取） |
| `Sci_HostWriteWords` | Sci_Upper.c:1061-1111 | 内部模拟主机写寄存器（用于模块间参数设置） |
| `Sci_IsAnyPortBusy` | Sci_Upper.c:1637-1649 | 查询所有串口是否忙碌 |

---

## 4. 寄存器地址映射

### 4.1 地址空间总览

```
0x1000 - 0x10FF  命令/控制区（写 0x06 触发复位/功能开关等）
0x1100 - 0x11FF  保留区（含系统功能 ON/OFF 0x1102/0x1103）
0x2000 - 0x20FF  校准系数区（K/B 对，47 通道 × 2 = 94 字）
0x2100 - 0x21FF  保护参数区（65 字）
0x2200 - 0x22FF  其他参数区（SOC 表 42 字 + 32 字铜损 + RTC 12 字 = 86 字）
0x2300 - 0x23FF  可扩展参数区（OtherElement，32 字）
0xC000           LCD 数据区
0xC001           FA RTC 区
0xC002           序列号/硬件版本/软件版本（只读）
0xC008           事件记录区（100 条）
0xD000 - 0xDFFF  实时数据区（RO，114 字）
  0xD000: 电芯电压/温度/SOC 等（63 字）
  0xD100: RTC 时间 + 故障记录 + 错误标志 + 系统状态（33 字）
  0xD200: Cortex 故障快照（2 字）+ SOC_TEST 填充（16 字）
0xFFFD           Flash 连接触发
0xFFF0 - 0xFFF2  序列号/硬件版本/软件版本（可写）
```

### 4.2 读区域映射表（`s_reg_regions`，Sci_Upper.c:13-21）

| 区域 | 起始地址 | 字数 | 说明 |
|------|----------|------|------|
| 校准系数 | `RS485_ADDR_RW_CALIB` (0x2000) | `KB_NUM × 2` (94) | K/B 系数对 |
| 保护参数 | `RS485_ADDR_RW_PORTECT` (0x2100) | 65 | 保护阈值和滤波 |
| 其他参数 | `RS485_ADDR_RW_OTHER` (0x2200) | 86 | SOC 表 + 铜损 + RTC |
| 可扩展参数 | `RS485_ADDR_RW_OTHER_CANADD` (0x2300) | 32 | OtherElement |
| AFE 参数 | `RS485_ADDR_RW_AFE_PARAMETER` | 24 | AFE 配置参数 |
| LCD 数据 | `RS485_ADDR_RO_LCD` (0xC000) | — | 通过 `s_lcd_entries` 查表 |
| 实时数据 | `RS485_ADDR_RO_START0` (0xD000) | 114 | 实时监控数据 |

### 4.3 写命令分发表（`s_wr_dispatch`，Sci_Upper.c:670-689）

| 地址范围 | 处理函数 |
|----------|----------|
| 0x2100 起 65 字 | `Sci_WrRegs_0x10_Protect` |
| 0x2300 起 32 字 | `Sci_WrRegs_0x10_OtherElement` |
| 0x2200 起 2 字 | `Sci_WrRegs_0x10_SocTable` |
| 0x2200+偏移（RTC） | `Sci_WrRegs_0x10_RTC` |
| 0xFFF0-0xFFF3 | `Sci_WrRegs_0x10_SN_Version` |
| 0xFFFD | `Sci_WrRegs_0x10_FlashConnect` |

### 4.4 0x06 写命令地址（Sci_Upper.c:397-443）

| 地址 | 命令 |
|------|------|
| 0x1000 | 复位校准系数 |
| 0x1001 | 复位保护记录 |
| 0x1002 | 复位保护参数 |
| 0x1003 | 复位 OtherElement |
| 0x1005 | 设置 SOC |
| 0x1006 | 复位 AFE 参数 |
| 0x1007 | 复位事件记录 |
| 0x1102 | BMS 功能 ON |
| 0x1103 | BMS 功能 OFF |

---

## 5. 缓冲区管理与溢出保护

### 5.1 缓冲区定义

```c
// Sci_Upper.h:7-8
#define SCI_TX_BUF_LEN       251
#define RS485_MAX_BUFFER_SIZE 251
```

`RS485MSG` 结构体中包含 `u16Buffer[251]`（实际为 `UINT8` 数组），同时作为接收缓冲区和发送缓冲区使用。另有一个独立的发送临时缓冲区 `g_u8SCITxBuff[251]`（Sci_Upper.c:54）。

### 5.2 溢出保护措施

**接收阶段（`Sci_ModbusProtocolFeed`，Sci_Upper.c:1135-1227）：**

1. **入口检查**（行 1140-1144）：`ptr_no >= RS485_MAX_BUFFER_SIZE` 时直接重置
2. **0x03/0x06 固定帧**（行 1180-1185）：收到第 8 字节（index=7）即标记帧完成
3. **0x10 可变帧**（行 1187-1211）：
   - 在 `ptr_no == 6` 时读取字节数字段 `u16Buffer[6]`，计算 `u16FrameEndIndex = byte_count + 8`
   - 检查 `u16FrameEndIndex >= RS485_MAX_BUFFER_SIZE` 则丢弃（行 1191-1195）
   - 每收一字节都重复校验（行 1197-1211）
4. **通用尾部检查**（行 1220-1224）：`ptr_no++` 后再次检查是否越界

**读寄存器阶段（`Sci_Deal_ReadRegs_0x03`，Sci_Upper.c:371-378）：**

```c
u16ReadByteNum = u16RegCount << 1;
if ((u16RegCount == 0U) ||
    (u16ReadByteNum > (UINT16)(RS485_MAX_BUFFER_SIZE - 5U)))
{
    // 错误：数据量超限
}
```

**0x06 写入接口（`Sci_HostWriteWords`，Sci_Upper.c:1068-1069）：**

```c
if ((u16Count << 1) > (UINT16)(RS485_MAX_BUFFER_SIZE - 9U))
{
    return RS485_ERROR_DATA_INVALID;
}
```

### 5.3 安全快照回滚机制

写保护参数或 OtherElement 时，先拷贝当前值到栈上的 `snapshot` 数组，写入失败时回滚（Sci_Upper.c:1746-1753, 1798-1805）。

---

## 6. CRC16 计算

CRC16 实现位于 `PubFunc.c:118-144`：

```c
UINT16 Sci_CRC16RTU(UINT8 *pszBuf, UINT8 unLength)
{
    UINT16 CRCC = 0xFFFF;
    for (CRC_count = 0; CRC_count < unLength; CRC_count++)
    {
        CRCC = CRCC ^ *(pszBuf + CRC_count);
        for (i = 0; i < 8; i++)
        {
            if (CRCC & 1) { CRCC >>= 1; CRCC ^= 0xA001; }
            else          { CRCC >>= 1; }
        }
    }
    return CRCC;
}
```

这是标准的 **Modbus CRC-16** 算法：
- 初始值 `0xFFFF`
- 多项式 `0xA001`（CRC-16/Modbus，即 `x^16 + x^15 + x^2 + 1` 的反转形式）
- 低位优先（LSB first）逐位处理

**CRC 校验流程（`CRC_verify`，Sci_Upper.c:299-316）：**

1. 取帧长 `ptr_no - 2`（去掉末尾 2 字节 CRC）
2. 提取接收帧中的 CRC 值（低字节在前，高字节在后）
3. 计算帧体的 CRC16
4. 比较：匹配则 `AckType = RS485_ACK_POS`，否则返回 `RS485_ERROR_CRC_ERROR`

**CRC 追加（`Sci_ACK_0x03`，Sci_Upper.c:980-982）：**

```c
u16SciTemp = Sci_CRC16RTU((UINT8 *)s->u16Buffer, i);
s->u16Buffer[i++] = u16SciTemp & 0x00FF;   // CRC 低字节
s->u16Buffer[i++] = u16SciTemp >> 8;        // CRC 高字节
```

### ⚠️ CRC16 参数类型隐患

`Sci_CRC16RTU` 的第二个参数 `unLength` 类型为 `UINT8`，最大值 255。而 `RS485_MAX_BUFFER_SIZE` 为 251，帧头 + 数据 + CRC = 251，数据部分最大 249 字节，在 UINT8 范围内。当前设计下不会溢出，但如果未来增大缓冲区需注意。

---

## 7. 潜在问题分析

### 7.1 ⚠️ `Sci_CRC16RTU` 长度参数为 `UINT8`，限制最大 255 字节

**位置：** `PubFunc.c:118`

`unLength` 类型为 `UINT8`，在当前 251 字节缓冲区下安全，但如果未来扩展缓冲区大小超过 255 字节，CRC 计算将截断。建议改为 `UINT16`。

### 7.2 ⚠️ `u16Buffer` 类型为 `UINT8[]` 但字段名含 `u16`

**位置：** `Sci_Upper.h:116`

`struct RS485MSG` 中 `u16Buffer[RS485_MAX_BUFFER_SIZE]` 实际是 `UINT8` 数组（字节数组），但命名 `u16` 容易误导。这是历史遗留命名。

### 7.3 ⚠️ ISR 中直接操作硬件寄存器，无临界区保护

**位置：** `Sci_PortIRQHandler`（Sci_Upper.c:1402-1491）

`Sci_PortService` 在主循环中读取 `u8FramePending` 标志，该标志在 ISR 中被设置（行 1438）。虽然 `u8FramePending` 是 `UINT8`（原子写入），但 `Sci_PortService` 中后续对协议上下文的读写没有临界区保护。如果 ISR 在 `Sci_PortService` 执行期间再次触发并修改同一上下文，可能导致竞态条件。当前设计通过 `pstUsart->CR1` 禁用 RXNE 中断（行 1439）来缓解此问题，但如果 IDLE 中断在处理帧期间触发，仍有理论风险。

### 7.4 ⚠️ `fputc` 调试输出与 USART1 共用引脚

**位置：** `Sci_Upper.c:2090-2112`

`fputc` 使用 USART1（PB6/PB7 remap）作为调试输出，与 Modbus 通信的 `g_stSciPort1` 共用同一 USART 外设。如果 `printf` 在 ISR 或帧处理期间被调用，会干扰 Modbus 收发。当前通过 `SCI_DEBUG_UART_TX_WAIT_LOOP` 限流，但在高波特率场景下可能造成冲突。

### 7.5 ⚠️ `Sci_ModbusProtocolFeed` 中 `ptr_no` 自增后二次越界检查

**位置：** Sci_Upper.c:1219-1224

```c
s->ptr_no++;
if (s->ptr_no >= RS485_MAX_BUFFER_SIZE)
{
    Sci_ModbusResetMessage(s);
    return 0;
}
```

在 0x03/0x06 命令路径中，`ptr_no` 在 `== 7` 时已标记帧完成并 `return 1`（行 1183-1184），不会再执行到行 1219。但在 0x10 命令路径中，如果 `u16FrameEndIndex` 计算值刚好等于 `RS485_MAX_BUFFER_SIZE - 1`，`ptr_no` 自增到 `RS485_MAX_BUFFER_SIZE` 后触发重置。这个逻辑是正确的，但依赖于 `u16FrameEndIndex` 的计算精确无误。

### 7.6 ⚠️ 校准系数写入函数为空实现

**位置：** `Sci_WrRegs_0x10_CalibCoef`（Sci_Upper.c:1713-1715）

该函数体为空，意味着通过 Modbus 写入校准系数实际上不会执行任何操作（既不成功也不报错）。这可能是一个未完成的功能或有意为之的安全设计。

### 7.7 ⚠️ `Sci_WrRegs_0x10_RTC` 为空实现

**位置：** `Sci_Upper.c:1766-1768`

RTC 时间写入为空实现，与校准系数类似。

### 7.8 ⚠️ `Sci_ACK_0x03_ReadRegs_Data` 中硬编码的结构体偏移

**位置：** Sci_Upper.c:781

```c
u16SciTemp = *(&g_stCellInfoReport.u16VCell[0] + j);
```

通过指针算术直接访问 `g_stCellInfoReport` 结构体内部字段，假设布局连续。如果 `stCell_Info` 结构体定义发生变化（增删字段），这段代码会产生隐蔽的语义错误。

### 7.9 ⚠️ `Sci_CopyProductIdBytes` 中 `byte_count` 未限制上界

**位置：** Sci_Upper.c:516-529

```c
for (i = 0; i < PRODUCT_ID_LENGTH_MAX; ++i)
{
    dst[i] = (i < byte_count) ? (UINT8)s->u16Buffer[7 + i] : 0U;
}
```

循环上界为 `PRODUCT_ID_LENGTH_MAX`（32），`byte_count` 来自 Modbus 帧中的字节计数字段。如果上位机发送的 `byte_count` 大于 32，循环仍以 32 为上界，不会越界。但 `byte_count` 的值会被写入 `*length`，如果调用方信任该值，可能导致逻辑错误。

### 7.10 ⚠️ Flash 写入失败的回滚可能触发副作用

**位置：** Sci_Upper.c:1749-1753, 1801-1805

当 `EEPROM_SaveRWParametersToFlash()` 失败时，代码回滚 `snapshot` 并调用 `Sci_ApplyProtectSideEffects` / `Sci_ApplyOtherElementSideEffects`。这些副作用函数会触发 `InitData_SOC()` 等操作，即使数据实际未改变。这可能导致不必要的 SOC 重新计算。

---

## 附录：调用关系概览

```
主循环
 └─ App_CommonUpper()                    [Sci_Upper.c:2075]
     └─ Sci_PortService(&g_stSciPortN)   [Sci_Upper.c:1493]
         ├─ [ISR 已设置 u8FramePending]
         ├─ pfProcessFrame()              → Sci_ModbusProcessFrame()  [Sci_Upper.c:1229]
         │   ├─ CRC_verify()              [Sci_Upper.c:299]
         │   ├─ Sci_Deal_ReadRegs_0x03()  [Sci_Upper.c:357]
         │   ├─ Sci_Deal_WrReg_0x06()     [Sci_Upper.c:392]
         │   ├─ Sci_Deal_WrRegs_0x10()    [Sci_Upper.c:691]
         │   ├─ Sci_ACK_0x03()            [Sci_Upper.c:926]
         │   └─ Sci_ACK_0x06_0x10()       [Sci_Upper.c:989]
         ├─ pfGetTxBuffer()               → 返回 u16Buffer
         ├─ pfGetTxLength()               → 返回 AckLenth
         └─ Sci_PortStartTx()             [Sci_Upper.c:1342]

USARTx_IRQHandler
 └─ Sci_PortIRQHandler()                  [Sci_Upper.c:1402]
     ├─ 错误处理 → Sci_PortHandleError()   [Sci_Upper.c:1387]
     ├─ RXNE → pfRxFeed()                 → Sci_ModbusProtocolFeed() [Sci_Upper.c:1135]
     │   └─ 帧完整时设置 u8FramePending = 1
     ├─ IDLE → pfOnRxIdle()               → Sci_ModbusOnRxIdle()     [Sci_Upper.c:1290]
     └─ TXE  → 逐字节发送 pu8TxBuffer[]
         └─ TC  → Sci_PortFinishTx()       [Sci_Upper.c:1359]
             └─ pfOnTxComplete()           → Sci_ModbusOnTxComplete() [Sci_Upper.c:1300]
```
