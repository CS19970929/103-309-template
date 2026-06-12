# SH367309 AFE 驱动模块分析

> 源码版本: 103 + 309 模板工程  
> 分析范围: I2C_AFE1.c/h, SH367309_Func.c/h, SH367309_DataDeal.c/h  
> 生成日期: 2026-06-12

---

## 1. 模块概览

### 1.1 AFE 芯片简介

本项目使用 **矽力杰 SH367309** 作为模拟前端 (AFE)，负责：
- 最多 16 串电芯电压采集
- 3 路 NTC 温度采集
- 电流采集 (CADC)
- 硬件级保护（过压/欠压/过流/短路/温度）
- 充放电 MOS 驱动控制
- 均衡控制

### 1.2 通信方式

MCU 与 SH367309 之间通过 **GPIO 模拟 I2C** (TWI) 通信：
- **SCL**: PB6
- **SDA**: PB7
- **从机地址**: `0x34` (`AFE_ID`)

### 1.3 文件职责划分

| 文件 | 职责 |
|------|------|
| `I2C_AFE1.c` | 底层 I2C 时序、CRC8、MTP 读写、AFE 初始化、电压/温度/电流数据采集 |
| `I2C_AFE1.h` | 寄存器地址宏定义、AFEDATA/SH367309_Read 结构体、GPIO 宏 |
| `SH367309_Func.c` | AFE 控制逻辑：复位/休眠/就绪检查/状态监控/故障映射/MOS 控制 |
| `SH367309_Func.h` | 保护参数宏定义、寄存器位域 union/struct、MTP 默认值计算 |
| `SH367309_DataDeal.c` | AFE ROM 参数管理：参数刷新/写入/校验/RS485 接口/Flash 存取 |
| `SH367309_DataDeal.h` | ROM 参数结构体、RS485 地址定义、保护参数默认值宏 |

### 1.4 架构层次

```
┌─────────────────────────────────────────────────────────┐
│  应用层:  DataDeal.c  →  MonitorAFE()                   │
│           Sci_Upper.c →  RS485 上位机通信                │
├─────────────────────────────────────────────────────────┤
│  功能层:  SH367309_Func.c                                │
│           App_SH367309() → Monitor + Config 刷新        │
│           Fault_ChangeToMCU() → 故障映射                 │
│           SH367309_UpdataAfeConfig() → 参数写入 AFE     │
├─────────────────────────────────────────────────────────┤
│  数据层:  SH367309_DataDeal.c                            │
│           Refresh_Parameters() → 参数计算               │
│           Write_Parameters() → EEPROM 写入              │
│           ReadEEPROM_AFE_Parameters() → 参数恢复       │
├─────────────────────────────────────────────────────────┤
│  驱动层:  I2C_AFE1.c                                    │
│           TwiWrite/TwiRead → I2C 时序                   │
│           MTPWrite/MTPRead → MTP 寄存器操作             │
│           UpdateVoltageFromBqMaximo() → 数据采集        │
├─────────────────────────────────────────────────────────┤
│  硬件层:  PB6(SCL), PB7(SDA), PB8(PWR?), PB9(DATA?)   │
│           GPIO 模拟 I2C (SH367309 @ 0x34)              │
└─────────────────────────────────────────────────────────┘
```

---

## 2. I2C GPIO 模拟实现

### 2.1 GPIO 引脚配置

**SCL (PB6) 和 SDA (PB7)** 在 `initAFE1_IIC()` 中配置为推挽输出模式（`I2C_AFE1.c` 第 660-669 行），速率 2MHz。初始化时两线均拉高。

头文件 `I2C_AFE1.h` 第 71-88 行定义了 GPIO 操作宏（两种风格并存，注释掉的寄存器直接操作方式被 GPIO 库函数方式替代）：

| 宏名 | 作用 | 实现 |
|------|------|------|
| `TWI_CLK_HIGH/LOW` | SCL 输出高/低 | `PBout(8) = 1/0` (寄存器级位操作) |
| `TWI_DAT_HIGH/LOW` | SDA 输出高/低 | `PBout(9) = 1/0` |
| `TWI_RD_CLK` | 读取 SCL 电平 | `PBin(8)` |
| `TWI_RD_DAT` | 读取 SDA 电平 | `PBin(9)` |
| `TWI_CLK_OUT/IN` | SCL 方向切换 | 直接操作 `GPIOB->CRH` |
| `TWI_DAT_OUT/IN` | SDA 方向切换 | 直接操作 `GPIOB->CRH` |

### 2.2 I2C 时序函数

所有 I2C 时序通过软件 bit-bang 实现，时钟周期约 4us（`Delay4us()`，`I2C_AFE1.c` 第 105-119 行）。

| 函数 | 位置 | 功能 |
|------|------|------|
| `TwiStart()` | 第 208-217 行 | 产生 START 条件：SDA↓ while SCL=H |
| `TwiReStart()` | 第 219-227 行 | 产生 ReSTART 条件 |
| `TwiStop()` | 第 229-240 行 | 产生 STOP 条件：SDA↑ while SCL=H |
| `TwiSendData()` | 第 279-352 行 | 发送 1 字节 + 等待 ACK |
| `TwiGetData()` | 第 360-394 行 | 接收 1 字节 + 发送 ACK/NACK |
| `TwiChkClkRelease()` | 第 248-270 行 | 等待从机释放 SCL（超时 1000 次，约 4ms） |

### 2.3 通信协议特点

SH367309 的 I2C 协议与标准 I2C 有差异：

**写操作** (`TwiWrite()`, 第 406-451 行):
```
[START] [SlaveID W] [RegAddr] [Data0] [Data1] ... [CRC8] [STOP]
```
- 每次写入附带 CRC8 校验字节
- CRC 计算范围: SlaveID + RegAddr + Data

**读操作** (`TwiRead()`, 第 464-533 行):
```
[START] [SlaveID W] [RegAddr] [Length] [ReSTART] [SlaveID R] [Data0] ... [DataN] [CRC8] [STOP]
```
- 读之前先写入地址和长度
- ReSTART 后切换为读模式
- 读取结束后验证 CRC8 校验
- CRC 计算范围: SlaveID + RegAddr + Length + ReadSlaveID + Data

---

## 3. 关键函数列表

### 3.1 I2C_AFE1.c 关键函数

| 函数 | 行号 | 说明 |
|------|------|------|
| `Delay4us()` | 105-119 | 微秒级延时（双层循环，72MHz 时钟基准） |
| `CRC8cal()` | 129-140 | 查表法 CRC8 计算 |
| `TwiStart()` | 208-217 | I2C 起始信号 |
| `TwiReStart()` | 219-227 | I2C 重复起始信号 |
| `TwiStop()` | 229-240 | I2C 停止信号 |
| `TwiChkClkRelease()` | 248-270 | 检查 SCL 释放（超时保护） |
| `TwiSendData()` | 279-352 | 发送 1 字节数据并检查 ACK |
| `TwiGetData()` | 360-394 | 接收 1 字节数据并发送 ACK |
| `TwiWrite()` | 406-451 | 写多字节（含 CRC） |
| `TwiRead()` | 464-533 | 读多字节（含 CRC 校验） |
| `MTPWrite()` | 540-569 | 写 MTP RAM 寄存器（>0x40），单字节写入，重试 1 次 |
| `MTPWriteROM()` | 574-603 | 写 MTP ROM 寄存器（0x00-0x19），单字节写入，40ms 写入等待 |
| `MTPRead()` | 607-636 | 读 MTP 寄存器，失败重试 1 次 |
| `InitAFE1_Sleep()` | 639-658 | AFE 睡眠/唤醒模式切换 |
| `initAFE1_IIC()` | 660-669 | 初始化 I2C GPIO |
| `InitAFE1()` | 678-704 | AFE 完整初始化流程 |
| `UpdateVoltageFromBqMaximo()` | 718-752 | **核心数据采集函数**：读取电压/温度/电流 |

### 3.2 SH367309_Func.c 关键函数

| 函数 | 行号 | 说明 |
|------|------|------|
| `AFE_Reset()` | 50-73 | AFE 软件复位：写入 0xC0 到 0xEA |
| `AFE_Sleep()` | 76-80 | 进入休眠模式 |
| `AFE_IDLE()` | 84-88 | 进入 IDLE 模式 |
| `AFE_IsReady()` | 98-125 | 等待 AFE 就绪（检查 VADC 转换完成，超时 1s） |
| `AFE_CheckStatus()` | 128-137 | 读取 BSTATUS1-3 状态寄存器 |
| `SH367309_Enable_AFE_Wdt_Cadc_Drivers()` | 146-159 | 使能 CADC，配置充放电 MOS 硬件控制 |
| `SH367309_SC_DelayT_Set()` | 162-197 | 设置短路保护延时 |
| `SH367309_DriverMos_Ctrl()` | 199-220 | MOS 开关控制（预充/充电/放电） |
| `SH367309_RecordFaultOnActive()` | 222-236 | 故障边沿检测与记录 |
| `Fault_ChangeToMCU()` | 238-315 | **核心故障映射**：AFE 状态 → g_stCellInfoReport |
| `App_SH367309_Monitor()` | 317-374 | 周期性监控：读取状态 + 故障处理 |
| `App_SH367309()` | 376-380 | 主入口：Monitor + Config 刷新 |

### 3.3 SH367309_DataDeal.c 关键函数

| 函数 | 行号 | 说明 |
|------|------|------|
| `Choose_Right_Value()` | 42-53 | 查表匹配最近等级（用于参数转换） |
| `Refresh_Parameters()` | 55-129 | 将 RS485 参数转换为 AFE 寄存器值 |
| `Write_Parameters()` | 161-196 | 写入 25 字节 ROM 寄存器并回读校验 |
| `SH367309_UpdataAfeConfig()` | 198-264 | **核心配置函数**：比较→写入→复位→使能 |
| `Sci_WrRegs_0x10_AFE_Parameters()` | 266-304 | RS485 Modbus 0x10 写多个寄存器 |
| `Sci_WrReg_0x06_Reset_AFE_Parameters()` | 306-322 | RS485 Modbus 0x06 复位参数到默认 |
| `Sci_ACK_0x03_RW_AFE_Parameters()` | 324-338 | RS485 Modbus 0x03 读取参数 |
| `EEPROM_ResetData_AFE_ParametersToDefault()` | 340-359 | 恢复默认参数并保存到 Flash |
| `ReadEEPROM_AFE_Parameters()` | 361-390 | 从 Flash 加载参数（含范围校验） |

---

## 4. 寄存器映射 (MTP Registers)

### 4.1 ROM 寄存器 (0x00 - 0x19) — EEPROM 持久存储

定义于 `I2C_AFE1.h` 第 10-18 行，结构体定义于 `SH367309_DataDeal.h` 第 94-191 行。

| 地址 | 宏名 | 含义 | 结构体 |
|------|------|------|--------|
| 0x00-0x01 | `SCONF1/SCONF2` | 系统配置：串数、均衡、MOS 控制模式 | `BYTE_00H_01H_TypeDef` |
| 0x02-0x03 | `OVT_LDRT_OVH/OVL` | 过压保护阈值+延时 | `BYTE_02H_03H_TypeDef` |
| 0x04-0x05 | `UVT_OVRH/OVRL` | 过压恢复+欠压延时 | `BYTE_04H_05H_TypeDef` |
| 0x06-0x07 | `UV/UVR` | 欠压保护阈值+恢复 | `BYTE_06H_07H_TypeDef` |
| 0x08-0x09 | `BALV/PREV` | 均衡开启电压+预充电压 | `BYTE_08H_09H_TypeDef` |
| 0x0A-0x0B | `L0V/PFV` | 低电压禁止充电+二次过充保护电压 | `BYTE_0AH_0BH_TypeDef` |
| 0x0C-0x0D | `OCD1V_OCD1T/OCD2V_OCD2T` | 两级放电过流保护阈值+延时 | `BYTE_0CH_0DH_TypeDef` |
| 0x0E-0x0F | `SCV_SCT/OCCV_OCCT` | 短路保护+充电过流保护 | `BYTE_0EH_0FH_TypeDef` |
| 0x10 | `MOST_OCRT_PFT` | MOS 延时+过流恢复+过充恢复时间 | `BYTE_10H_TypeDef` |
| 0x11-0x19 | `OTC/OTCR/UTC/UTCR/OTD/OTDR/UTD/UTDR/TR` | 温度保护阈值+参考电阻 | `BYTE_11H_19H_TypeDef` |

完整 ROM 参数结构体为 `AFE_ROM_PARAMETERS_TypeDef`（`SH367309_DataDeal.h` 第 180-191 行），共 26 字节。

### 4.2 RAM 寄存器 (0x40 - 0x72) — 实时读写

定义于 `I2C_AFE1.h` 第 20-49 行：

| 地址 | 宏名 | 含义 |
|------|------|------|
| 0x40 | `MTP_CONF` | 配置寄存器：IDLE/SLEEP/WDT/CADC/MOS 控制 |
| 0x41-0x42 | `MTP_BALANCEH/L` | 均衡状态（高/低字节） |
| 0x43 | `MTP_BSTATUS1` | 状态1：OV/UV/OCD1/OCD2/OCC/SC/PF/WDT |
| 0x44 | `MTP_BSTATUS2` | 状态2：UTC/OTC/UTD/OTD |
| 0x45 | `MTP_BSTATUS3` | 状态3：DSG_FET/CHG_FET/PCHG_FET/L0V/EEPR_WR/DSGING/CHGING |
| 0x46-0x4A | `MTP_TEMP1/2/3` | 温度 ADC 值（各 2 字节） |
| 0x4C | `MTP_CUR` | 电流 ADC 值（2 字节） |
| 0x4E-0x6C | `MTP_CELL1-16` | 16 路电芯电压 ADC 值（各 2 字节） |
| 0x6E | `MTP_ADC2` | ADC2 值 |
| 0x70-0x71 | `MTP_BFLAG1/2` | 附加状态标志 |
| 0x72 | `MTP_RSTSTAT` | 复位状态 |

### 4.3 读取数据结构体

`AFEDATA`（`I2C_AFE1.h` 第 51-58 行）与硬件寄存器地址一一对应：
```c
typedef struct _AFEDATA_ {
    UINT16 Temp1;      // 0x46-0x47: 温度1
    UINT16 Temp2;      // 0x48-0x49: 温度2
    UINT16 Temp3;      // 0x4A-0x4B: 温度3
    INT16  Cur1;       // 0x4C-0x4D: 电流
    UINT16 Cell[16];   // 0x4E-0x6D: 16路电芯电压
    INT16  Cadc;       // ...: 电流ADC
} AFEDATA;
```

### 4.4 状态寄存器位域

`MTP_REG_BSTATUS1`（`SH367309_Func.h` 第 152-165 行）：
```
bit 0: OV    — 过压保护状态
bit 1: UV    — 欠压保护状态
bit 2: OCD1  — 放电过流1状态
bit 3: OCD2  — 放电过流2状态
bit 4: OCC   — 充电过流状态
bit 5: SC    — 短路保护状态
bit 6: PF    — 二次过充保护状态
bit 7: WDT   — 看门狗复位标志
```

`MTP_REG_CONF`（`SH367309_Func.h` 第 198-211 行）：
```
bit 0: IDLE    — IDLE 模式控制
bit 1: SLEEP   — 休眠模式控制
bit 2: ENWDT   — 看门狗使能
bit 3: CADCON  — CADC 使能
bit 4: CHGMOS  — 充电 MOS 控制
bit 5: DSGMOS  — 放电 MOS 控制
bit 6: PCHMOS  — 预充 MOS 控制
bit 7: OCRC    — 输出CRC使能
```

---

## 5. CRC8 计算

### 5.1 CRC8 查找表

位于 `I2C_AFE1.c` 第 70-86 行，256 字节标准 CRC8 查找表。

多项式: 从表内容推断为 `x^8 + x^2 + x + 1`（CRC-8/MAXIM 或类似变体）。

### 5.2 CRC8 计算函数

`CRC8cal()`（`I2C_AFE1.c` 第 129-140 行）：

```c
UINT8 CRC8cal(UINT8 *p, UINT8 Length) {
    UINT8 crc8 = 0;
    for (; Length > 0; Length--) {
        crc8 = CRC8Table[crc8 ^ *p];
        p++;
    }
    return crc8;
}
```

标准查表法实现，初始值为 0，逐字节查表异或。

### 5.3 CRC 在通信中的使用

**写操作**：CRC 覆盖 `[SlaveID, RegAddr, Data...]`，写入时作为最后一个字节发送。

**读操作**：CRC 覆盖 `[SlaveID, RegAddr, Length, ReadSlaveID, Data...]`，读取后进行校验比对。校验失败则返回 0（错误），数据不拷贝到输出缓冲区。

---

## 6. 温度查找表

### 6.1 AFE 专用 10K NTC 查找表

位于 `I2C_AFE1.c` 第 8-66 行：

```c
static const UINT16 iSheldTemp_10K_AFE[LENGTH_TBLTEMP_AFE_10K] = {  // 56 个元素 = 28 对
    // AD(kΩ*100)    (Temp+40)*10
    11611, 100,   // -30°C
    8935,  150,   // -25°C
    6943,  200,   // -20°C
    ...
    98,    1400,  // 100°C
};
```

- **X 轴**: 电阻值（单位: kΩ × 100），即 `Rt × 100`
- **Y 轴**: 温度值（单位: (T + 40) × 10），即 `(T + 40) × 10`
- **覆盖范围**: -30°C ~ 105°C，共 28 个温度点
- **格式**: 交错存储 [X1, Y1, X2, Y2, ...]

### 6.2 SH367309 芯片级 NTC 查找表

位于 `SH367309_Func.c` 第 8-22 行：

```c
const UINT16 iSheldTemp_10K_NTC[141] = {
    20375, 19204, 18115, 17100, ... , 98
};
```

- 141 个元素，覆盖 -40°C ~ 100°C
- 每个元素对应 1°C 步进的电阻值
- 用于将温度阈值转换为 AFE 寄存器值（`Refresh_Parameters()` 中使用）

### 6.3 温度转换公式

`UpdateVoltageFromBqMaximo()` 第 733-741 行：

```c
// Step 1: 从 ADC 值计算电阻值
u32temp = (TR_ResRef × TempN) / (32769 - TempN);
// TR_ResRef = 680 + 5 × TR（单位: kΩ × 100）

// Step 2: 限幅
UPDNLMT16(u32temp, 65535, 0);

// Step 3: 查表得到温度值
u16TempBat[i] = GetEndValue(iSheldTemp_10K_AFE, LENGTH_TBLTEMP_AFE_10K, u32temp);
```

温度查找表使用 `GetEndValue()`（`PubFunc.c` 第 15 行）进行线性插值。

---

## 7. 数据流：从 AFE 到 g_stCellInfoReport

### 7.1 电压/温度/电流采集流程

```
UpdateVoltageFromBqMaximo()        [I2C_AFE1.c:718]
  │
  ├── MTPRead(MTP_TEMP1, sizeof(AFEDATA), &Registers_AFE1)  // 一次性读取 0x46-0x6D
  │     └── TwiRead(AFE_ID=0x34, addr, length, buf)
  │           └── GPIO 模拟 I2C 时序 + CRC8 校验
  │
  ├── 电压转换 (×5/32 → mV):
  │     u16VCell[i] = SwapEndian(Registers_AFE1.Cell[i]) * 5 >> 5
  │     → SH367309_Read_AFE1.u16VCell[0..15]
  │
  ├── 温度转换 (ADC → 电阻 → 查表 → °C):
  │     u32temp = (TR_ResRef × SwapEndian(Registers_AFE1.TempN)) / (32769 - SwapEndian(...))
  │     → GetEndValue(iSheldTemp_10K_AFE, 56, u32temp)
  │     → SH367309_Read_AFE1.u16TempBat[0..2]
  │
  └── 电流:
        SH367309_Read_AFE1.u16Current = SwapEndian(Registers_AFE1.Cadc)
```

### 7.2 故障状态映射流程

```
App_SH367309()                       [SH367309_Func.c:376]
  │
  ├── App_SH367309_Monitor()         [SH367309_Func.c:317]
  │     │
  │     ├── MTPRead(MTP_BALANCEH, 5, &u8_MTP_BALANCEH)
  │     │   // 读取 0x41-0x45: 均衡 + BSTATUS1/2/3
  │     │
  │     ├── SystemRuntime_SetMosStatus(CHG_FET, DSG_FET)
  │     │
  │     ├── Fault_ChangeToMCU()      [SH367309_Func.c:238]
  │     │     │
  │     │     ├── BSTATUS1.OV → g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp
  │     │     ├── BSTATUS1.UV → g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp
  │     │     ├── BSTATUS1.OCD1||OCD2 → b1IdischgOcp
  │     │     ├── BSTATUS1.OCC → b1IchgOcp
  │     │     ├── BSTATUS2.UTC → b1CellChgUtp
  │     │     ├── BSTATUS2.OTC → b1CellChgOtp
  │     │     ├── BSTATUS2.UTD → b1CellDischgUtp
  │     │     ├── BSTATUS2.OTD → b1CellDischgOtp
  │     │     ├── BSTATUS1.SC → System_ErrFlag.u8ErrFlag_CBC_DSG
  │     │     └── FaultWarnRecord2() → 故障记录
  │     │
  │     └── 检查 PF/WDT/EEPR_WR/L0V → System_ERROR_UserCallback()
  │
  └── SH367309_UpdataAfeConfig()    [SH367309_DataDeal.c:198]
        // 参数比较 + 写入 + 复位 + 重新使能
```

### 7.3 调用方

`UpdateVoltageFromBqMaximo()` 的调用者：

| 调用位置 | 文件 | 说明 |
|----------|------|------|
| `MonitorAFE(0, UpdateVoltageFromBqMaximo())` | `DataDeal.c:1168` | 主循环数据采集 |
| `rtc_sleep_afe_sh367309.c:13` | 同名文件 | 休眠唤醒后的数据采集 |

### 7.4 参数配置流程

```
RS485 上位机发送写命令
  │
  ├── Sci_WrRegs_0x10_AFE_Parameters()
  │     ├── 写入 AFE_Parameters_RS485_Struction（内存）
  │     ├── AFE_SaveCurValuesToFlash()  // 持久化到 Flash
  │     └── AFE_PARAM_WRITE_Flag = 1    // 触发下次 AFE 写入
  │
  └── App_SH367309() 下一次循环
        └── SH367309_UpdataAfeConfig()
              ├── Refresh_Parameters()   // 重新计算所有寄存器值
              ├── MTPRead(0x00, 25)      // 读当前 AFE 内容
              ├── 比较差异
              ├── 如果不同:
              │     ├── MCUO_AFE_VPRO = 1  // 拉高 VPRO 使能写
              │     ├── Write_Parameters() // 逐字节写入 + 回读校验
              │     ├── MCUO_AFE_VPRO = 0  // 拉低 VPRO
              │     ├── AFE_Reset()        // 复位使配置生效
              │     └── SH367309_Enable_AFE_Wdt_Cadc_Drivers()
              └── 如果相同:
                    └── 跳过写入
```

---

## 8. 潜在问题分析

### 8.1 I2C 通信可靠性

**问题 1: 缺少 SCL/SDA 方向切换保护**  
`TWI_DAT_OUT` / `TWI_DAT_IN` 等宏在头文件中被定义（第 77-78 行），但在 `TwiSendData()` 和 `TwiGetData()` 的某些路径中未严格保证方向切换时机。例如 `TwiSendData()` 第 332 行 `TWI_DAT_IN` 后直接开始循环读取 ACK，若总线竞争或从机异常，可能导致读取不稳定。

**问题 2: ACK 检测循环次数偏少**  
`TwiSendData()` 第 335-342 行 ACK 等待仅循环 10 次（约 40us），若从机响应慢或总线容性负载大，可能漏检。

**问题 3: MTPRead 仅重试 1 次**  
`MTPRead()` 第 625-629 行失败后仅重试 1 次。对于关键数据采集，重试次数偏少。

### 8.2 温度计算

**问题 4: 温度查找表不覆盖极端温度**  
AFE 专用表 `iSheldTemp_10K_AFE` 覆盖范围 -30°C ~ 105°C。若 NTC 开路（电阻极大）或短路（电阻极小），`GetEndValue()` 可能返回越界值。`UPDNLMT16` 限制了电阻值范围（0-65535），但未对最终温度值做范围检查。

**问题 5: 分母为零风险**  
温度计算 `u32temp = (TR_ResRef × TempN) / (32769 - TempN)`，当 `TempN = 32769` 时分母为零。虽然实际中 ADC 值极少达到此值，但缺少防御性检查。

### 8.3 MTP 写入

**问题 6: MTPWrite 无回读校验**  
`MTPWrite()` 写入后不回读校验，仅通过重试来处理错误。而 `MTPWriteROM()` 写入 EEPROM 时有 40ms 等待（模拟 EEPROM 编程周期），但同样无回读。`Write_Parameters()` 在 `SH367309_DataDeal.c` 第 161-196 行中有回读校验，这是正确的做法。

**问题 7: MTPWrite 返回值语义与系统不一致**  
`MTPWrite()` 第 539 行注释明确说明："这个返回值就不改了，和目前体系相反"。函数返回 1 表示成功、0 表示失败，但系统其他部分可能约定相反。`UpdateVoltageFromBqMaximo()` 第 726 行 `if (MTPRead(...))` 中 1=成功，但第 747-750 行 `result = 1` 时表示失败——这个约定是统一的，但需要维护者注意。

### 8.4 故障处理

**问题 8: UV 故障恢复时的 MOS 操作不安全**  
`Fault_ChangeToMCU()` 第 263-286 行，当 UV 故障恢复时直接操作 `GPIO_DC_EN` 和 `GPIO_2727_EN` 引脚拉高。这绕过了 AFE 的 MOS 控制逻辑，直接操作外部驱动电路，可能存在时序竞争。

**问题 9: AFE_Read 失败时无处理**  
`App_SH367309_Monitor()` 第 369-373 行，`MTPRead` 失败时只有注释"读取失败，要做点什么事情？进入深度休眠咯？"，实际无任何处理。

### 8.5 参数管理

**问题 10: Flash 存储地址硬编码**  
`E2P_ADDR_E2POS_AFE_Parameters = 3000`（`SH367309_DataDeal.h` 第 7 行），地址分配依赖硬编码，缺乏地址冲突检查机制。

**问题 11: AFE_Parameters_RS485_Struction 按 4 字节对齐访问**  
`Sci_ACK_0x03_RW_AFE_Parameters()` 第 334 行 `*(P + j * 4)` 和 `EEPROM_ResetData_AFE_ParametersToDefault()` 第 348 行 `*(P + i * 4 - 1)` 依赖 `AFE_Value_Typedef` 为 4 字节（2 × UINT16）。若结构体对齐方式改变，这些偏移计算将出错。

### 8.6 延时与性能

**问题 12: Delay4us 精度依赖编译器优化**  
`Delay4us()` 使用双层 `for` 循环实现延时（第 110-116 行），实际延时取决于编译器优化等级和系统时钟。注释标注 72MHz/24MHz，但未通过硬件定时器校准。

**问题 13: InitAFE1 中调用链过长**  
`InitAFE1()` 第 678-704 行依次调用：I2C 初始化 → close_ctlc → AFE 就绪等待 → 配置写入 → MOS 初始化 → 零点校准。整个流程中 `AFE_IsReady()` 可能阻塞最长 1 秒（50 次 × 20ms），若 AFE 硬件异常，会显著影响启动时间。

### 8.7 大小端处理

**问题 14: 大小端转换分散**  
`U16_SwapEndian()` 在 `UpdateVoltageFromBqMaximo()` 中对每个字段逐一调用（第 730-745 行）。SH367309 返回大端数据，MCU 为小端，转换正确但分散在多处。若 AFEDATA 结构体字段顺序变化，需逐个检查。

---

## 附录: A. 保护参数默认值

### 三元锂 (TERNARYLI) 保护参数

| 参数 | 值 | 单位 | 说明 |
|------|-----|------|------|
| 单节过压 | 4205 | mV | OVP |
| 单节过压恢复 | 4010 | mV | OVP 恢复 |
| 单节欠压 | 2800 | mV | UVP |
| 单节欠压恢复 | 2900 | mV | UVP 恢复 |
| 均衡开启电压 | 5000 | mV | — |

### 磷酸铁锂 (LIFEPO) 保护参数

| 参数 | 值 | 单位 | 说明 |
|------|-----|------|------|
| 单节过压 | 3750 | mV | OVP |
| 单节过压恢复 | 3650 | mV | OVP 恢复 |
| 单节欠压 | 2500 | mV | UVP |
| 单节欠压恢复 | 2600 | mV | UVP 恢复 |

### 温度保护参数 (通用)

| 参数 | 值 | 含义 |
|------|-----|------|
| `VAL_CHG_OTP` | 110 | 充电高温保护 (T > 70°C) |
| `VAL_CHG_OTP_RCV` | 105 | 充电高温恢复 (T < 65°C) |
| `VAL_CHG_UTP` | 35 | 充电低温保护 (T < -5°C) |
| `VAL_CHG_UTP_RCV` | 40 | 充电低温恢复 (T > 0°C) |
| `VAL_DSG_OTP` | 110 | 放电高温保护 (T > 70°C) |
| `VAL_DSG_OTP_RCV` | 105 | 放电高温恢复 (T < 65°C) |
| `VAL_DSG_UTP` | 35 | 放电低温保护 (T < -5°C) |
| `VAL_DSG_UTP_RCV` | 40 | 放电低温恢复 (T > 0°C) |

## 附录: B. 电流/电压等级表

### OCD1V/OCCV 等级表 (单位: mV)

`{20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 160, 180, 200}`

### OCD2V 等级表 (单位: mV)

`{30, 40, 50, 60, 70, 80, 90, 100, 120, 140, 160, 180, 200, 300, 400, 500}`

### SCV 短路电压等级表 (单位: mV)

`{50, 80, 110, 140, 170, 200, 230, 260, 290, 320, 350, 400, 500, 600, 800, 1000}`

### SCT 短路延时等级表 (单位: us)

`{0, 64, 128, 192, 256, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960}`
