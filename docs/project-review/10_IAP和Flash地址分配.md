# 10 — IAP 和 Flash 地址分配

## IAP 系统总体架构

BMS 项目涉及 **3 个 MCU** + **4 个固件**:

```
┌─────────────────────────────────────────────────────┐
│ 通信工具 MCU (STM32F103RET6, 512KB Flash)              │
│                                                         │
│  ┌──────────────────┐   ┌──────────────────┐           │
│  │ COMM_TOOL IAP    │   │ COMM_TOOL APP    │           │
│  │ 0x08000000       │──→│ 0x08008000       │           │
│  │ (32KB)           │   │ (64KB)           │           │
│  └──────────────────┘   └────────┬─────────┘           │
│                                  │                      │
│                   RS485/UART ←───┴───→ CAN (250kbps)   │
└──────────────────────────────────┼──────────────────────┘
                                   │
                    CAN 总线 (固件升级中转)
                                   │
┌──────────────────────────────────┼──────────────────────┐
│ BMS MCU (STM32F103RCT6, 256KB Flash)                    │
│                                  │                      │
│  ┌──────────────────┐   ┌───────┴──────────┐           │
│  │ IAP 引导程序     │   │ BMS APP 固件      │           │
│  │ (103C8-IAP)      │──→│ 0x08004800        │           │
│  │ 0x08000000       │   │ (94KB)            │           │
│  │ (18KB)           │   │                   │           │
│  └──────────────────┘   └───────────────────┘           │
│                                                         │
│  升级器 MCU (STM32F103C8, 64KB) — 外部编程器             │
└─────────────────────────────────────────────────────────┘
```

## BMS MCU — 完整 Flash 地址分配

**MCU**: STM32F103RCT6, 256KB Flash, 起始 `0x08000000`

| 起始地址 | 结束地址 | 大小 | 区域名称 | 内容 |
|---------|---------|------|---------|------|
| `0x08000000` | `0x080047FF` | 18 KB | IAP 引导 | CAN/UART IAP 引导程序 |
| `0x08004800` | `0x0801BFFF` | ~94 KB | APP 固件 | BMS 主应用程序 |
| `0x0801C000` | `0x0801C3FF` | 1 KB | AFE 参数 A | SH367309 AFE 校准参数 (24 words) |
| `0x0801C400` | `0x0801C7FF` | 1 KB | 读写参数 A | 保护参数 + OtherElement (121 words) |
| `0x0801C800` | `0x0801CBFF` | 1 KB | AFE 参数 B | AFE 参数 B 槽 |
| `0x0801CC00` | `0x0801CFFF` | 1 KB | 读写参数 B | 读写参数 B 槽 |
| `0x0801D000` | `0x0801D7FF` | 2 KB | 事件日志 A | 事件记录 (100 records × 2 words) |
| `0x0801D800` | `0x0801DFFF` | 2 KB | 事件日志 B | 日志 B 槽 |
| `0x0801E000` | `0x0801E7FF` | 2 KB | SOC 数据 A | SOC 状态快照 |
| `0x0801E800` | `0x0801EFFF` | 2 KB | SOC 数据 B | SOC B 槽 |
| `0x0801F000` | `0x0801F3FF` | 1 KB | 升级策略 | 升级参数策略版本号 |
| `0x0801F400` | `0x0801F7FF` | 1 KB | 工厂老化 | 老化时间 + 状态 |
| `0x0801F800` | `0x0801FBFF` | 1 KB | 更新标志 | IAP 请求标志 (Flash 方式，已废弃) |
| `0x0801FC00` | `0x0801FFFF` | 1 KB | 休眠标志 | 休眠模式 Boot Flag |

### 页大小

`FLASH_STORAGE_PAGE_SIZE = 0x800` (2 KB) for STM32F10X_HD (256-512KB Flash)。
中等密度 (MD) 设备为 `0x400` (1 KB)。

## SRAM 邮箱 (IAP ↔ APP 握手)

### 邮箱地址

**位置**: `0x20004FE0` — SRAM 最高 32 字节

**为什么选择此地址**:
- IAP 链接脚本限制 SRAM 使用到 `0x20004FE0` (20 KB - 32B)
- 软件复位 (`NVIC_SystemReset()`) 不清除 SRAM
- 邮箱位于 IAP 的 SRAM 边界之外，IAP 不会覆盖它

### 邮箱结构

**源文件**: IAP `boot_control.h:10-17`, APP `Flash.c:42-47`

```c
typedef struct {
    UINT32 magic;        // 0x49415031 ("IAP1")
    UINT32 magic_inv;    // ~magic = 0xB6AFBECE
    UINT32 request;      // 0x5AA55AA5
    UINT32 request_inv;  // ~request = 0xA55AA55A
    UINT32 crc;          // magic ^ request ^ 0xA5A55A5A
} BOOT_CTRL_MAILBOX;
```

偏移量:
| 偏移 | 字段 | 值 |
|------|------|-----|
| `0x20004FE0` | magic | `0x49415031` |
| `0x20004FE4` | magic_inv | `0xB6AFBECE` |
| `0x20004FE8` | request | `0x5AA55AA5` |
| `0x20004FEC` | request_inv | `0xA55AA55A` |
| `0x20004FF0` | crc | 计算值 |

## APP → IAP 跳转流程

### APP 侧 (BMS 固件)

**源文件**: `Flash.c`

```c
// Step 1: APP 发起跳转
APP_To_IAP_Jump()
  └── AppUpgrade_RequestIap()
        ├── 在 0x20004FE0 处填充邮箱
        └── 返回成功

// Step 2: 系统复位
__disable_fault_irq();
MCU_RESET();  // → NVIC_SystemReset()
```

### IAP 侧 (引导程序)

**源文件**: `103c8-iap/Source/main.c:34-71`

```c
int main(void) {
    SystemInit();
    if (BootCtrl_ShouldJumpToApp() != 0U) {
        // 无 IAP 请求 → 验证 APP 向量表 → 跳转到 APP
        IAP_To_APP_Jump();
    } else {
        // 收到 IAP 请求 → 留在 IAP 模式 → 等待固件
        // ... 初始化 IAP 外设 → while(1) 处理 CAN 帧
    }
}
```

### BootCtrl_ShouldJumpToApp() 逻辑

**源文件**: `103c8-iap/Source/boot_control.c:69-77`

```
1. 检查 SRAM 邮箱 0x20004FE0:
   if (magic == 0x49415031 && request == 0x5AA55AA5 && CRC 正确)
       → BootCtrl_ConsumeIapRequest() 清除邮箱
       → 返回 0 (不跳转到 APP, 留在 IAP)

2. 没有 IAP 请求:
   → CanIap_IsValidAppVector(0x08004800)
     → 检查 APP 向量表: SP 在 SRAM 范围内, PC 在 Flash 范围内
     → 返回 1 (跳转到 APP)
```

## IAP → APP 跳转流程

**源文件**: `103c8-iap/Source/main.c:235-266`

```c
void IAP_To_APP_Jump(void) {
    1. 验证 APP 在 0x08004800 处的向量表
    2. 从向量表 +4 读取复位 PC
    3. __disable_irq()
    4. 停止 TIM2、停止 UART、停止 CAN
    5. SCB->VTOR = 0x08004800      // 向量表重定位
    6. __set_MSP(*0x08004800)      // 主栈指针 = APP 初始 SP
    7. 调用 APP 复位处理程序
}
```

## CAN IAP 协议

**源文件**: `103c8-iap/Source/can_iap_protocol.h` (地址定义), `103c8-iap/Source/can_iap.c` (协议引擎)

### CAN ID 方案

| 帧类型 | CAN ID 格式 | 说明 |
|--------|-----------|------|
| 控制帧 | `0x14F8F000 \| node_id` | IAP 命令/响应 |
| 确认帧 | `0x14F8F100 \| node_id` | 数据 ACK/NACK |
| 数据帧 | `0x14000000 \| (seq << 8) \| node_id` | 固件数据块 |
| 心跳帧 | `0x05F` (标准 11-bit) | IAP 存活信号 |

### CAN IAP 命令

| 命令 ID | 命令 | 功能 |
|--------|------|------|
| 0x01 | HELLO | 握手，交换协议版本 |
| 0x02 | START | 传输开始：固件大小 + CRC + 擦除首页 |
| 0x03 | COMMIT | 数据块提交：序列 + 数据长度 + CRC |
| 0x04 | END | 传输结束：总帧数 + CRC 校验 + 写首页 + 复位 |

### 数据块结构

```
一个 DATA 块 = 32 帧 × 8 字节 = 256 字节
每帧: 8 字节数据 + 序列号 + CRC
全部块发送后 → COMMIT → END
```

### Flash 写入优化

**源文件**: `103c8-iap/Source/iap_flash.c`

- **首页缓冲**: 首页 (0x08004800) 先缓存在 SRAM 中，END 命令时才原子写入（防止向量表损坏）
- **常规写入**: 其他页接收后立即擦除 + 编程 + 回读验证
- **所有权标记**: 支持两个 "owner" (CAN 和 UART) 分别管理写入

## 通信工具 IAP

**MCU**: STM32F103RET6 (512KB Flash)

### Flash 布局

| 起始地址 | 大小 | 区域 | 说明 |
|---------|------|------|------|
| `0x08000000` | 32 KB | IAP 引导 | 通信工具自身 IAP |
| `0x08008000` | 64 KB | APP | 通信工具应用程序 |
| `0x08018000` | ~416 KB | 固件缓存 | 缓存其他 MCU 的固件镜像 |
| `0x0807F800` | 2 KB | 固件元数据 | 缓存固件的描述信息 |

### SRAM 邮箱

| 地址 | 魔数 | 用途 |
|------|------|------|
| `0x2000FFE0` | `0x43544950` ("CTIP") | 通信工具 IAP 握手 |

### 自升级

**源文件**: `ct_self_iap.c`

通信工具可以通过两种方式升级自己：
- **UART**: 上位机通过 Modbus 写寄存器 `0xFFFD` (连接) → `0xFFFE` (数据) → `0xFFFF` (完成)
- **CAN**: 收到 CAN 地址 `0x0E` 的 `ENTER_IAP` 命令（magic byte `0xC3, 0x3C`）

## 升级参数策略

**源文件**: `UpgradeParamPolicy.h`

### 策略版本

`PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION = 0x0618`

### 工作原理

1. APP 启动时读取 Flash `0x0801F000` 处的版本号
2. 如果版本号匹配 → 跳过（参数已是最新）
3. 如果版本号不匹配（新固件）→ 根据 `PROJECT_CFG_UPGRADE_PARAM_*` 宏执行相应重置操作

### 可重置的参数

| 宏 | 当前值 | 说明 |
|-----|--------|------|
| `UPGRADE_PARAM_RESET_AFE` | 0 | 重置 AFE 校准 |
| `UPGRADE_PARAM_RESET_PROTECT` | 0 | 重置保护参数 |
| `UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE` | 0 | 重置均衡开启电压 |
| `UPGRADE_PARAM_RESET_SOC_CONFIG` | 0 | 重置 SOC 配置 |
| `UPGRADE_PARAM_RESET_SOC_SNAPSHOT` | 0 | 重置 SOC 快照 |
| `UPGRADE_PARAM_RESET_EVENT_RECORD` | 0 | 清除事件日志 |
| `UPGRADE_PARAM_RESET_FACTORY_AGING_TIME` | 0 | 重置老化时间 |
| `UPGRADE_PARAM_UPDATE_OTHER_ELEMENT` | 0 | 用 Project_Config 覆盖 OtherElement |

## 升级流程完整时序

```
通信工具 MCU                      BMS MCU
     │                               │
     │── HELLO (CAN, 250kbps) ──────→│  (如果 APP 在线)
     │←── ACK ──────────────────────│
     │── ENTER_IAP ────────────────→│  (命令 APP 跳转)
     │                               │  APP_To_IAP_Jump()
     │                               │  └── 设置 SRAM 邮箱
     │                               │  └── NVIC_SystemReset()
     │                               │  IAP 启动
     │                               │  └── 检查邮箱 → 留在 IAP
     │── HELLO (CAN) ──────────────→│  (与 IAP 握手)
     │←── ACK ──────────────────────│
     │── START (size+CRC) ─────────→│  (擦除首页)
     │←── ACK ──────────────────────│
     │── DATA[0..31] ──────────────→│  (256 字节块)
     │── COMMIT (seq+crc) ─────────→│  (编程 Flash)
     │←── ACK ──────────────────────│
     │   ... (重复直到所有块发送)     │
     │── END (count+crc) ──────────→│  (验证 + 写首页 + 复位)
     │←── ACK ──────────────────────│
     │                               │  复位 → IAP 再次启动
     │                               │  └── 无 IAP 请求
     │                               │  └── 验证新 APP 向量
     │                               │  └── IAP_To_APP_Jump()
     │                               │  新固件运行
```

## 关键常量和地址汇总

| 常量 | 值 | 来源文件 |
|------|-----|---------|
| `FLASH_ADDR_IAP_START` | `0x08000000` | `Flash.h` (APP), `main.h` (IAP) |
| `FLASH_ADDR_APP_START` | `0x08004800` | `Flash.h` (APP), `main.h` (IAP) |
| `CAN_IAP_APP_LIMIT_ADDR` | `0x0801F800` | `can_iap_protocol.h` (IAP) |
| `BOOT_CTRL_MAILBOX_ADDR` | `0x20004FE0` | `boot_control.h` (IAP) |
| `BOOT_CTRL_MAILBOX_MAGIC` | `0x49415031` | `boot_control.h` (IAP) |
| `BOOT_CTRL_MAILBOX_REQUEST` | `0x5AA55AA5` | `boot_control.h` (IAP) |
| `FLASH_TO_IAP_VALUE` | `0x00AB` | `Flash.h` (APP, 已废弃) |
| `FLASH_TO_APP_VALUE` | `0xFFFF` | `Flash.h` (APP, 已废弃) |
| `IAP_FLASH_PAGE_SIZE` | `0x400` (1KB) | `iap_flash.c` (IAP) |
| IAP SRAM 限制 | `0x20004FE0` | `IAP_103_Plus.sct` |
| IAP CAN 基地址 | `0x14F8F000` | `can_iap_protocol.h` |
| IAP CAN 速率 | 250 kbps | `can_iap.c` |

## 待确认问题

1. "升级器 MCU" (`upgrader_mcu`) 的源文件位置（构建日志指向 `E:\TODO\`，不在仓库中）
2. 通信工具的 CAN IAP 预分频器值 (4) 与 BMS IAP 的预分频器值 (18) 不同，是否匹配？
3. SRAM 邮箱的 CRC 算法具体是什么？（`magic ^ request ^ 0xA5A55A5A` 似乎太简单）
4. 如果 APP 固件损坏（向量表无效），IAP 会如何处理？（应自动留在 IAP 等待升级）
