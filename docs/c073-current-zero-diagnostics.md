# C073 SH367309 启动零电流校准与诊断

分支：`fix/c073-current-zero-calibration-diagnostics`

## 目的

解决“出厂静置电流为 0，使用一段时间后静置出现约 0.2~0.4 A，深度休眠唤醒后仍未被校准”的现场问题，并保留足够诊断信息用于串口/Modbus 上位机定位。

原实现存在两个关键隐患：

1. SH367309 CADC 约 4 Hz（约 250 ms/次），原 warm-start 仅等待 120 ms，随后每 20 ms 重复读寄存器，可能在新 CADC 样本产生前就完成所谓“稳定采样”。
2. 原实现即使没有达到完整确认样本数，循环结束时只要 `sample_cnt > 0` 也可能接受结果。

## 启动校准模型

不再区分 cold/warm 参数。普通上电、普通复位、深睡后的 MCU reset 都使用完全相同的启动校准流程。

校准只允许每次 MCU 启动执行一次：状态初始为 NOT_RUN；第一次 InitAFE1() 执行校准后状态变为 VALID 或明确失败码。同一次运行中后续 AFE 故障恢复再次调用 InitAFE1() 时，校准函数直接返回，不会重新学习零点。深睡流程会 MCU reset，因此下一次唤醒状态重新回到 NOT_RUN，会自然重新校准。

统一流程：

1. CTL 保持关闭。
2. 强制 CADCON=1，CHGMOS/DSGMOS/PCHMOS=0。
3. 回读 MTP_CONF，并读取 BSTATUS3，确认控制位和实际 FET 均关闭。
4. 等待 300 ms，读取第一笔独立 CADC。
5. 再次确认 FET 关闭。
6. 再等待 300 ms，读取第二笔独立 CADC。
7. 两笔均满足 |raw| <= 40 count，且 |raw2-raw1| <= 6 count 时校准成功。
8. 成功：零点采用两笔平均值，内部使用 raw x4 保存以保留 0.5 count 分辨率。
9. 失败：zero=0，不应用错误偏移；deadband 仍保持产品原有 200 mA。
10. 成功：同样维持本项目原有 200 mA deadband。

15A 配置（3 个 2 mΩ 并联）下约 1 count = 14 mA，因此：
- 40 count ≈ 0.56 A
- 6 count ≈ 0.084 A

## Modbus 诊断区

新增独立只读寄存器：`0xC100 ~ 0xC10F`，不修改任何已有地址。

| 地址 | 含义 | 类型/单位 |
|---|---|---|
| C100 | 诊断版本 | uint16，当前 1 |
| C101 | 启动零点状态 | uint16，见下表 |
| C102 | flags | bit0=校准有效；bit1=本次为深睡启动；bit2=当前充电；bit3=当前放电；bit4=校准无效/使用 fallback |
| C103 | boot raw1 | int16，CADC count |
| C104 | boot raw2 | int16，CADC count |
| C105-C106 | zeroRawX4 | int32，校准零点 count×4 |
| C107 | runtimeRaw | int16，当前 CADC 原始 count |
| C108-C109 | correctedRawX4 | int32，扣除零点后的 count×4 |
| C10A-C10B | current_mA | int32，有符号；正=充电，负=放电 |
| C10C | deadband_mA | uint16 |
| C10D | 校准期间最后一次 MTP_CONF | uint8 放在低 8 位 |
| C10E | 校准期间最后一次 BSTATUS3 | uint8 放在低 8 位 |
| C10F | 调试注入 flags | bit0=运行电流 raw 注入；bit1=启动零点 raw 注入；正式版本通常为 0 |

### C101 状态码

| 值 | 状态 |
|---:|---|
| 0 | NOT_RUN |
| 1 | VALID |
| 2 | CONFIG_WRITE_ERROR |
| 3 | CONFIG_READBACK_ERROR |
| 4 | SAMPLE_READ_ERROR |
| 5 | FET_ACTIVE |
| 6 | UNSTABLE |
| 7 | OUT_OF_RANGE |

## 现场判断方法

### 1. 校准成功，但之后静态偏移再次出现

典型表现：
- C101 = 1
- C103/C104 接近且代表启动时真实零漂
- C105-C106 合理
- 随时间/温度变化，C107 明显偏离启动 raw
- C108-C109 因此也产生偏移

这更像硬件/AFE/采样链路的温漂或随时间漂移，不是启动校准未执行。此时再评估“静止条件下的慢速运行零点跟踪”，而不是盲目扩大启动校准范围。

### 2. 深睡唤醒后校准没有成功

直接看 C101：
- 5：FET 实际未关闭，不允许学习零点。
- 6：两笔 300 ms 独立样本变化超过 6 count。
- 7：静态偏移超过 ±40 count，或者校准期间存在真实电流。
- 2/3/4：AFE/I2C 通信或寄存器读写问题。

### 3. 校准成功但上报电流仍异常

比较：
- C107：原始 CADC
- C105-C106：学习零点
- C108-C109：扣零后的 raw
- C10A-C10B：最终 mA

可以快速区分“AFE 原始值异常”“零点学习异常”“修正算法异常”“mA 换算/显示异常”。

## 建议测试

至少记录以下工况的 C100-C10F：
- 冷启动，无充放电
- 深度休眠后唤醒，无充放电
- 静置 10 min / 1 h
- 约 0.5 A、1 A、5 A、10 A 放电
- 板温较低/常温/较高时静置

如果现场再次出现 0.2~0.4 A，优先保留整组 C100-C10F，而不是只记录最终显示电流。


## 现有上位机单体电压页快速诊断

诊断显示上电/复位后默认关闭，不保存到 Flash。通过 Modbus 0x06 功能开关临时控制：

- 写 `0x1102 = 0x0007`：打开 V25~V32 电流诊断显示。
- 写 `0x1103 = 0x0007`：关闭诊断显示，立即恢复正常单体数据显示。
- 深睡/复位/重新上电后自动回到关闭状态。

打开后，为了无需修改旧上位机协议解析，Modbus D000 回包会把关键电流诊断量临时映射到 V25~V32。内部 `g_stCellInfoReport.u16VCell[]` 不被修改，因此 CAN、保护、SOC 仍使用原始单体数据。SH367309 平台最大 16 串，V25~V32 不对应真实单体。

| 上位机单体位置 | 显示值 | 解码 |
|---|---|---|
| V25 | zero status | 直接读取状态码 |
| V26 | boot raw1 + 1000 | raw1 = 显示值 - 1000 |
| V27 | boot raw2 + 1000 | raw2 = 显示值 - 1000 |
| V28 | zero raw x4 + 10000 | zeroRawX4 = 显示值 - 10000 |
| V29 | runtime raw + 1000 | runtimeRaw = 显示值 - 1000 |
| V30 | corrected raw x4 + 10000 | correctedRawX4 = 显示值 - 10000 |
| V31 | 充电电流 mA | 直接读取；1 LSB = 1 mA，非充电时为 0 |
| V32 | 放电电流 mA | 直接读取绝对值；1 LSB = 1 mA，非放电时为 0 |

例如：
- 充电 +350 mA：V31=350，V32=0。
- 放电 -853 mA：V31=0，V32=853。
- 静置且进入 200 mA deadband：V31=0，V32=0。

旧上位机仍可能把 V31/V32 的单位标签显示为 mV，但数值本身直接代表 mA，不再需要加减偏置换算。V31/V32 使用 uint16，因此该临时显示通道最大为 65535 mA；C073 量程足够。完整有符号电流诊断仍保留在 C10A-C10B（int32 mA），deadband_mA 仍保留在 C10C。

完整诊断仍保留在 C100~C10F；V25~V32 仅是 Modbus 上位机显示层的临时映射，不写回全局单体数组。


## AFE 电流 raw 调试注入

调试注入默认关闭，不影响正式版本：

```c
#define PROJECT_CFG_AFE_CURRENT_DEBUG_INJECT_ENABLE 0
```

需要测试时改为 1。启用宏后可通过代码调用，也可在 Keil Watch 中直接修改全局变量：

```c
AfeCurrent_DebugInjectRaw(100, AFE_CURRENT_DEBUG_INJECT_RUNTIME);
AfeCurrent_DebugInjectClear();
```

或直接观察/修改：

```c
g_afeCurrentDebugInject.raw
g_afeCurrentDebugInject.targetMask
```

targetMask：
- 0x01：只替换运行期 CADC raw，适合测试 raw→零点修正→K/B→deadband→SOC→串口整条链。
- 0x02：只替换启动零点采样 raw，真实 FET/MTP/BSTATUS 安全检查仍保留。
- 0x03：运行期和启动零点都替换。

raw 的类型是 int16 CADC count，正负方向与 SH367309 原始 CADC 一致。注入只发生在电流处理入口，不改写共享的 SH367309 实际采样缓存，因此其它 AFE 数据仍是真实硬件值。C10F 可确认当前是否仍处于注入模式。
