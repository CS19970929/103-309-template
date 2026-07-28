# 分口充电器识别与满充关断设计（2026-07-28）

## 1. 修改范围

本次只修改：

- `103 + 309/Project/Source/IO_Control.c`
- `103 + 309/Project/Source/IO_Control.h`

未修改 AFE、GPIO、采样、保护、温度、SOC、休眠和通信驱动。

现有 `Drivers_Ctrl()` 仍先计算充放电保护允许值。原 `bms_status` 主状态机继续负责 `S_IDLE/S_STARTUP/S_DSG/S_CHG` 的负载识别和放电 MOS 控制。新增状态机只是充电 MOS 的子状态机，只能在原保护结果上继续禁止充电，不能把保护已经关闭的充电 MOS 重新打开。

## 2. 调用关系

```text
App_AFEGet()                         200 ms，且本轮 AFE 采样成功
  -> DataLoad_CellVolt()
  -> DataLoad_Current()
  -> SH_AFE_GetProtectStatus()
  -> App_MOS_Relay_Ctrl()
       -> RefreshData_Drivers()
       -> Drivers_Ctrl()
            生成原有充电/放电保护允许值
       -> Drivers_External_Ctrl()
            -> 原 bms_status 状态机处理负载和放电 MOS
            -> ChargeCtrl_Step()
            -> 最终充电请求 = 原保护允许值 && 状态机允许值
            -> SH367309_DriverMos_Ctrl()
                 CHG 开：AFE CHGMOS=1，M-CCC=高
                 CHG 关：AFE CHGMOS=0，M-CCC=低
```

## 3. GPIO 和有效电平依据

| 信号 | 软件定义 | 初始化/控制依据 | 有效含义 |
| --- | --- | --- | --- |
| CHG-DET | PA9，`GPIO_CHG_DET/PIN_CHG_DET` | `conf/conf_gpio.h`、`conf/conf.c` 配置为浮空输入；`is_charger_online()` 读取低电平 | 低电平表示检测到充电器，只在充电 MOS 关闭时可信 |
| DSG-DET | PA8，`GPIO_DSG_DET/PIN_DSG_DET` | `conf/conf_gpio.h`、`conf/conf.c` 配置为浮空输入；`is_load_online()` 读取低电平 | 低电平表示检测到负载；继续由原 `bms_status` 状态机决定放电 MOS |
| M-CCC | PB14，`GPIO_M_CCC/PIN_M_CCC` | 初始化先拉低；`SH367309_DriverMos_Ctrl(GPIO_CHG, ...)` 中开时置高、关时置低 | 高电平允许充电 MOS，低电平禁止充电 MOS |

上述结论来自当前源码，没有用示波器或原理图复核。浮空输入对外部上下拉和干扰敏感，CHG-DET/DSG-DET 的硬件上下拉必须在样机上确认。

## 4. 状态转换

| 状态 | 充电 MOS 请求 | 转入条件 | 转出条件 |
| --- | --- | --- | --- |
| `WAIT_CHARGER` | 关 | 上电、探测确认拔出、故障恢复 | MOS 关闭时 CHG-DET 连续低 1 s，转 `CHARGING` |
| `CHARGING` | 开 | 可靠 CHG-DET 插入确认、回充确认、探测确认仍在线 | 满充转 `FULL_HOLD`；连续 5 min 无明确净充电且总压低于 78.0 V，转 `PROBE_OFF`；保护转 `FAULT_HOLD` |
| `PROBE_OFF` | 关 | 需要主动确认在线 | 非阻塞等待端口稳定 1 s，转 `PROBE_SAMPLE` |
| `PROBE_SAMPLE` | 关 | 端口已稳定 | CHG-DET 连续低 1 s，确认在线并重新充电；连续高 1 s，确认拔出并转等待 |
| `FULL_HOLD` | 持续关 | 正常满充成立 | 总压不高于 77.0 V、最高单体不高于 4.05 V、CHG-DET 可靠为低，三者持续 60 s 后才回充 |
| `FAULT_HOLD` | 关 | 原保护禁止、AFE 原始充电故障或关键采样数据无效 | 故障/数据恢复后转 `WAIT_CHARGER`，重新要求 CHG-DET 插入确认 |

CHG-DET 在 `CHARGING` 中不参与在线判断，因此充电 MOS 打开后 CHG-DET 变高不会导致抖动。

## 5. 满充判定

原 4250 mV 单体过充阈值未修改，仍是独立硬件/软件保护条件。

正常满充采用两个路径：

1. 常规恒压末端：
   - 总压不低于 79.0 V；
   - 最高单体不低于 4.18 V；
   - 当前不是净放电；
   - 净充电电流不高于 1.0 A；
   - 连续保持 60 s。
2. 充电器与负载同时工作时的后备路径：
   - 总压和最高单体满足同一满充电压区；
   - 不依赖净电流方向；
   - 连续保持 10 min。

满充成立后进入 `FULL_HOLD` 并立即关闭充电 MOS。轻微电压回落、CHG-DET 波动和电流波动均不会重新打开 MOS。

## 6. 集中参数及初始值

参数集中在 `IO_Control.h`，当前按 19 串、79.8 V 充电器设置（79.8 V / 19 = 4.20 V）。

| 参数 | 初始值 | 说明 |
| --- | ---: | --- |
| `CHARGE_CTRL_TARGET_SERIES` | 19 串 | 串数不匹配时按数据无效处理并禁止充电 |
| `CHARGE_CTRL_CHARGER_PACK_CV` | 79.80 V | 充电器标称值，当前只用于配置说明 |
| `CHARGE_CTRL_FULL_PACK_CV` | 79.00 V | 正常满充总压下限 |
| `CHARGE_CTRL_FULL_CELL_MV` | 4180 mV | 正常满充最高单体下限，与 79.0 V 总压条件形成独立判据 |
| `CHARGE_CTRL_FULL_TAPER_CURRENT_A10` | 1.0 A | 常规满充截止净充电电流 |
| `CHARGE_CTRL_FULL_TAPER_CONFIRM_MS` | 60 s | 常规满充保持时间 |
| `CHARGE_CTRL_FULL_VOLT_CONFIRM_MS` | 10 min | 带负载后备满充保持时间 |
| `CHARGE_CTRL_RECHARGE_PACK_CV` | 77.00 V | 回充总压上限 |
| `CHARGE_CTRL_RECHARGE_CELL_MV` | 4050 mV | 回充最高单体上限 |
| `CHARGE_CTRL_RECHARGE_CONFIRM_MS` | 60 s | 回充条件保持时间 |
| `CHARGE_CTRL_CHARGE_EVIDENCE_A10` | 0.5 A | 明确净充电在线证据 |
| `CHARGE_CTRL_PROBE_MAX_PACK_CV` | 78.00 V | 高于该电压不主动探测，避免接近充电器电压时误判 |
| `CHARGE_CTRL_PROBE_IDLE_MS` | 5 min | 无明确净充电后才允许主动探测 |
| `CHARGE_CTRL_PROBE_SETTLE_MS` | 1 s | 关 MOS 后端口稳定时间 |
| `CHARGE_CTRL_PROBE_SAMPLE_MS` | 1 s | CHG-DET 高电平确认时间 |
| `CHARGE_CTRL_DET_CONFIRM_MS` | 1 s | CHG-DET 低电平确认时间 |

这些是安全偏保守的首版值，不是电芯/充电器最终量产标定值。量产前需要按充电器恒压精度、线损、满充截止电流、电芯压差和客户允许的回充深度调整。

## 7. 保护和其他功能影响

- 过充、充电过流、充电高低温、MOS 高温、压差异常以及 AFE 原始充电故障均优先进入 `FAULT_HOLD`。
- 新逻辑不会主动打开原保护已经关闭的充电 MOS。
- 原 `bms_status` 对放电 MOS 的赋值和 `is_load_online()` 转换顺序已保留，原短路、放电过流、欠压和负载移除逻辑保持原调用链。
- 当前 `__FUNC__HEAT__` 未启用；若以后启用，`Heat_Cool.c` 中仍存在直接开关充电 MOS 的旧代码，会与“单一最终写入口”冲突，需要另行评审后改造。
- `Flash.c` 只在升级流程中直接关闭充电 MOS，属于安全方向动作，本次未改。

## 8. 场景检查

| 场景 | 结果 |
| --- | --- |
| 未插充电器上电 | `WAIT_CHARGER`，充电请求为 0 |
| 负载接入 | 原 `S_IDLE/S_STARTUP -> S_DSG` 路径保持，放电 MOS 由原逻辑打开 |
| 负载断开 | 原 `S_DSG -> S_IDLE` 路径保持，下一控制周期关闭放电 MOS |
| CHG-DET 短暂低 | 不足 1 s 不开 MOS |
| CHG-DET 稳定低 | 建立会话并打开 MOS |
| MOS 打开后 CHG-DET 变高 | `CHARGING` 锁存，不依据该信号关 MOS |
| 仅充电器充电 | 净充电证据持续刷新，不主动探测 |
| 充电器和负载同时存在、净充电 | 同上 |
| 充电器和负载同时存在、净放电 | 不判定拔出；总压低于 78.0 V 且持续 5 min 后做一次低频主动探测 |
| 充电过程拔掉充电器 | 低于 78.0 V 时最迟约 5 min 后主动探测并关闭；高压区保持 `UNKNOWN`，避免误判 |
| 79.8 V 附近 CHG-DET 高 | 不主动探测，不误判拔出 |
| 小电流恒压末端 | 电压区 + 小电流保持 60 s 后满充关闭 |
| 带负载导致净放电 | 满充电压区保持 10 min 后走后备满充关闭 |
| 满充后充电器不拔 | `FULL_HOLD` 持续关闭 |
| 满充后轻微回落 | 未同时低于两个回充阈值，不回充 |
| 满充后深度回落且充电器仍接入 | 两个电压阈值 + CHG-DET 低保持 60 s 后回充 |
| 达到 4250 mV 或其他充电保护 | 原保护优先关断，并进入 `FAULT_HOLD` |

## 9. 现有硬件无法消除的限制

充电 MOS 打开后，当前硬件只有“净电流”，无法分别测量充电器支路和负载支路电流。负载电流大于充电电流时，软件不能仅凭净放电判断充电器是否仍物理连接。

因此本实现采用：

- 会话锁存；
- 电流只作为“明确在线”正证据；
- 低频、非阻塞主动关断探测；
- 接近 79.8 V 时返回 `UNKNOWN`，不猜测拔出。

这意味着高压区拔掉充电器后，软件可能暂时保持会话为 `UNKNOWN`，直到满充关断、保护关断、复位或电压离开禁止探测区。若客户要求任何电压和任意负载组合下都立即、无 MOS 动作地识别拔枪，必须增加独立充电口电压/支路电流检测硬件。

## 10. 尚需确认的跨模块安全项

`App_AFEGet()` 在 AFE 读取失败时通过 `ChargeCtrl_ForceOff()` 进入 `FAULT_HOLD`，并在 M-CCC 仍为高时立即执行只关充电 MOS 的安全动作。该入口不修改放电 MOS，不等待下一次正常 AFE 数据。

## 11. 构建与静态检查

- 工程：`CommomSH367309_16series_103RCT6_C.uvprojx`
- 目标：`Target 1`
- 工具链：Keil ARMCC 5.06 update 7
- 构建结果：0 Error，39 Warning（detached 干净提交完整重编译）
- 修改文件 `IO_Control.c`、`DataDeal.c`：0 Error，0 Warning
- 固件大小：Code 50488 B，RO-data 2372 B，RW-data 1208 B，ZI-data 6016 B
- 产物：AXF 891236 B，BIN 53068 B
- `git diff --check`：通过

39 个警告均位于未修改的历史文件，本次没有通过屏蔽警告规避问题。
