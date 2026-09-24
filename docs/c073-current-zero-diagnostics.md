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
9. 失败：zero=0，不应用错误偏移；运行 deadband 使用 500 mA。
10. 成功：维持本项目原有 200 mA deadband。

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
| C10F | 保留 | 0 |

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

为了无需立即修改旧上位机协议解析，诊断分支同时把关键电流诊断量镜像到未使用的 V25~V32。当前项目只有 7 串，且电压最大/最小/总压计算仅遍历实际 SeriesNum，因此这些槽位不参与任何保护或电压统计。

| 上位机单体位置 | 显示值 | 解码 |
|---|---|---|
| V25 | zero status | 直接读取状态码 |
| V26 | boot raw1 + 1000 | raw1 = 显示值 - 1000 |
| V27 | boot raw2 + 1000 | raw2 = 显示值 - 1000 |
| V28 | zero raw x4 + 10000 | zeroRawX4 = 显示值 - 10000 |
| V29 | runtime raw + 1000 | runtimeRaw = 显示值 - 1000 |
| V30 | corrected raw x4 + 10000 | correctedRawX4 = 显示值 - 10000 |
| V31 | current mA + 30000 | current_mA = 显示值 - 30000；正=充电，负=放电 |
| V32 | deadband mA | 直接读取 |

例如 V31 显示 29600，表示当前电流为 -400 mA；显示 30350，表示 +350 mA。

完整诊断仍保留在 C100~C10F，V25~V32 只是便于直接使用现有上位机观察。
