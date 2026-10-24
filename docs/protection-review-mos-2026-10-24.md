# STM32F103 + SH367309 软件/硬件保护审查与 MOS 高温保护改进

日期：2026-10-24  
审查基线：`new-new-a036-c073-master` @ `d59fc90ff36ccd6e408432dcc0ccb70f979e7c4f`  
审查范围：`103 + 309/Project/Source`；代码审查与静态差异核对，**未完成 Keil ARMCC5 编译、真实硬件测温、MOS 波形及故障注入测试**。

## 1. 实际保护控制链路

- 主循环 `main.c`：`App_SysTime()` → `App_WarnCtrl()` → `App_AFEGet()` → `App_AnlogCal()`。这是裸机任务，无 RTOS。
- 温度获取：`ADC.c` 由 PA1/ADC1 DMA 采 MOS NTC，`ADC_TTC()` 将 ADC 值用 `iSheldTemp_10K[]` 查表并低通滤波，输出 `g_i32ADCResult[ADC_TEMP_MOS1]`。ADC 当前只实际配置了一路 MOS 温度，虽保留 MOS2 的代码及变量。
- 温度上报：`DataDeal.c/DataLoad_Temperature()` 经 K/B 标定，以 `(°C + 40) * 10` 编码写入 `g_stCellInfoReport.u16Temperature[MOS_TEMP1]`，并进行 `Monitor_TempBreak` 检查。正常 AFE 获取路径受 200ms 时基和串口发送占用限制。
- 软件保护：`Fault.c/App_WarnCtrl()` 对过压、欠压、电流、温度、压差等使用阈值+计数器并产生二/三级标志；`App_MosOtp_SecondCheck()`、`App_MosOtp_ThirdCheck()` 使用 `App_PubOPUPChk()` 滞回判断。
- MOS 决策：`IO_Control.c/RefreshData_Drivers()` 将三级故障送入 `Driver_Element.Fault_Flag`；`IODrivers.c/MosCtrl_SameDoor_NoPreChg()` 汇总 CHG/DSG 许可及强制控制。
- 硬件执行：本板 `Driver_Element.u8_DriverCtrl_Right=1`，最终由 `IO_Control.c/Drivers_External_Ctrl()` 调 `SH367309_DriverMos_Ctrl()` 写 SH367309 `MTP_CONF` 的 CHGMOS/DSGMOS，并控制 MCU CTLC 引脚。
- AFE 自主保护：`SH367309_DataDeal.c/Refresh_Parameters()` 以 `AFE_Parameters_RS485_Struction` 为基础更新硬件 OV/UV、OCD/OCC、SC、OTC/UTC/OTD/UTD 等寄存器，并比较和写回 AFE EEPROM；`SH367309_Func.c/App_SH367309_Monitor()` 读取 BSTATUS、FET 状态和部分保护事件。

## 2. 软件保护与硬件保护矩阵

| 对象 | MCU 软件 | SH367309 硬件 | 说明 |
|---|---|---|---|
| 单体过充/过放 | 分级阈值、滤波、故障记录；控制充放 MOS | OV/UV 阈值、延时与恢复 | 需测实物电芯保护时序及寄存器 |
| 充放电过流 | 电流量测与二/三级保护、重复触发关断 | OCC、OCD1、OCD2 | AFE 与软件等级不是简单一一对应 |
| 短路 | 获取 SC 故障，记录/联动 | SCV、SCT 快速保护 | 不应依赖 MCU 200ms 软件周期作为短路主保护 |
| 电芯/AFE NTC 高低温 | CHG/DSG 各自 OTP/UTP 软件判断 | OTC、UTC、OTD、UTD | 两套温度阈值分别设置；AFE 是独立电芯温度输入 |
| MOS 散热片高温 | PA1 MOS NTC，二级告警、三级切断 CHG/DSG | 当前未发现独立 MOS NTC 硬件比较 | 必须确保 MCU 故障时的风险由系统安全分析覆盖 |
| NTC 断线 | `Monitor_TempBreak()` + MOS 有效性关断 | AFE NTC 各由其自身电路判断 | 断线范围需用实物开/短路注入验证 |
| AFE 通信故障 | `MonitorAFE()` 计数、恢复及强制 MOS 关断 | AFE 内部保护继续运行需以实际配置验证 | 不能把“AFE 尚有保护”当作通信异常可忽略 |
| RTC/STOP | 软件需要唤醒后重新采样/再做保护 | AFE 在自身供电条件下工作 | 唤醒前 MCU ADC 未就绪时不得重开 MOS |

`SH367309_DataDeal.h` 中的 AFE 参数初始默认值，例如充电高温 60°C/恢复 50°C、放电高温 70°C/恢复 60°C；`Fault.h` 的软件电芯温度默认阈值和 MOS 阈值不同。实际硬件阈值必须以 **AFE EEPROM/寄存器读回** 为准；软件默认值还受外部 EEPROM 加载影响。化学体系的 `LIFEPO` 预处理条件也必须核查构建定义。

## 3. MOS 高温保护的本轮改动

默认参数在 `Fault.h`：二级触发 85°C、二级恢复 75°C；三级触发 95°C、三级恢复 80°C；滤波计数 100。系统 `TIM3_IRQHandler` 每 1ms 驱动基本计数，`b1Sys10msFlag5` 每 10ms 出现一次，因此 `100` 次正常约 1 秒（不包含 MCU/ADC/AFE 调度阻塞影响）。

本轮复用既有 `App_MosOtp_SecondCheck()` 和 `App_MosOtp_ThirdCheck()`：保留原软件保护事件/滤波/历史记录，不改参数 EEPROM 结构、通讯地址或枚举布局。新增 `MosTempProtection_IsBlocked()` 作为仅关断优先的安全许可：

1. 采样条件：DMA ADC 值必须位于 MOS NTC 查表合法范围内（严格 `326 < adc < 3771`），同时已产生有效滤波值和已上报的标定值。开/短路、冷启动和 STOP 后 ADC 尚未建立时禁止释放 MOS。
2. 配置条件：三级触发必须高于恢复阈值，且均位于可用温度编码范围内；配置非法时禁止释放 MOS。
3. 快速热关断：实时 ADC 温度或已标定上报温度达到三级阈值立即置快速锁存。锁存保持到两路温度都低于/等于恢复阈值，不允许在略低于触发温度时立刻重新开管。
4. 故障/恢复：若已有软件三级高温故障则仍由原有 100 次滤波确认恢复；若传感器无效则拒绝执行第二/三级故障恢复滤波；`ERROR_TEMP_BREAK` 同样禁止开管。
5. 最终执行：在 `IODrivers.c` 的 `FORCE_OPEN_MODE` 覆盖之后再关断 CHG/DSG；`Drivers_External_Ctrl()`、`SH367309_DriverMos_Ctrl()` 和 `SH367309_Enable_AFE_Wdt_Cadc_Drivers()` 的输出侧均做拦截。
6. 长串口传输：`App_AnlogCal()` 的 ADC 1ms 路径在发现禁止条件时直接将 `MCUO_AFE_CTLC` 拉至关断状态，降低等待 AFE 200ms 主流程的风险。实际 CTLC→FET 关断延迟必须示波器验证。

注意：低于 1 秒的短暂高温可能只触发快速锁存而没有软件三级保护历史记录，这是一项 **待改进的诊断可观测性**，并非已记录的保护事件。

## 4. 明确发现的其他风险（本分支未修改）

- **P0 参数越界**：`SH367309_DataDeal.c/Refresh_Parameters()` 用 `AFE_TEMPERATURE[i]` 直接索引 `iSheldTemp_10K_NTC[141]`，但来自 RS485 的温度参数限制范围可能允许编码 /10 超出 140，潜在越界读取。应先将传入温度编码转换后做有序性与下标范围校验，再编程 AFE。
- **P0/P1 强制开管优先级**：原 `IODrivers.c` 在安全状态综合之后有普通 `FORCE_OPEN_MODE` 覆盖。当前只强化 MOS OTP 的不可覆盖关断；其余过压/欠压/过流在不同方向允许恢复通路，要按同口 MOS 导通特性明确哪些允许、哪些绝对禁止，避免一刀切引入无法充电恢复。
- **P1 EEPROM 参数错误处理**：`EEPROM.c` 将 EEPROM 值载入 `PRT_E2ROMParas` 后检查范围，但超界时主要标故障，没有直接恢复安全默认配置或保障所有保护参数之间的排序。本轮只给 MOS 高温阈值加了关断侧顺序守卫。
- **P1 AFE 通信/配置失败窗口**：`DataDeal.c/MonitorAFE()` 通过若干次通信失败后才升级故障；`SH367309_UpdataAfeConfig()` 写失败重试时必须实测 MOS 维持状态及 AFE 硬件保护是否继续可用。
- **P1 MCU 失效时 MOS 高温缺少独立硬件兜底**：PA1 是 MCU ADC，不属于 SH367309 的 OTC/OTD NTC。若要求 MCU 死锁也必须热关断，需要外置比较器/热开关/硬件锁断或设计安全诊断。
- **P1 采样新鲜度**：目前可检测首次 ADC 未就绪、查表范围外、开短路极值；仍没有独立 DMA 转换新鲜度超时监控。若 DMA 卡死而最后数值恰好正常，软件可能无法及时识别。
- **P2 启动序列假设**：`System_FUNC_StartUp()` 现有有效实现直接 `return STARTUP_OVER`；不要把 #if0 中的旧启动延迟逻辑当作现行开管前提。
- **P2 诊断与追踪**：建议将“快速热锁存”、“三级软件高温”、“传感器无效”、“参数无效”、“最后温度采样年龄”区分输出，供生产测试、RTC 唤醒分析使用。

## 5. 必测用例与验收依据

| 用例 | 预期 |
|---|---|
| 冷启动正常室温 | ADC 校准未建立之前 CHG、DSG/CTLC 保持关断；采样建立、无其它故障后允许原有开管流程 |
| 上电时 MOS 已高于 95°C | 全程保持关断，不能出现开管脉冲 |
| MOS 温度缓慢升至 85°C | 二级告警按滤波生效，仍由原业务逻辑决定输出 |
| MOS 温度瞬间达到 95°C | 快速锁存关断；降至 94°C 仍不恢复，降至 80°C 且有效后允许根据原三级故障状态恢复 |
| 持续 95°C 超过软件确认时间 | 三级 MOS 高温故障/历史记录产生，充放 MOS 关闭 |
| 三级故障后缓慢降至 80°C | 软件恢复计数满足后故障位清除；不能仅靠快速锁存被释放而提前开管 |
| 强制开 CHG、DSG 同时 MOS 过热 | 外部强制开管命令不得绕过安全关断 |
| 拔掉或短接 PA1 NTC | 传感器异常，禁止自动恢复；恢复接线后重新经过正常保护流程 |
| 运行中 NTC 采样越界、坏 EEPROM 阈值 | MOS 关断优先，不可被普通 FORCE_OPEN 覆盖 |
| 长时间串口发送导致 AFE 主任务延迟 | ADC 快速关断路径仍执行；在示波器上量化关断时间 |
| RTC/STOP 唤醒 | ADC 清空、尚未重新建立时不得误开管；关注 WATCHDOG、CTLC 和 SH367309 FET 状态 |
| AFE 复位/通信错误/EEPROM 参数更新 | 整个过程中没有超温误开管，且原 AFE OV/UV/OCD/OCC/SC 保护继续符合配置 |

建议采集：PA1 模拟电压、STM32 CTLC、AFE CHGMOS/DSGMOS 寄存器、CHG/DSG 栅极及 PACK 端状态；同步采集原始 ADC、滤波温度、标定温度、软件二/三级故障、快速热锁存、温度断线状态。

## 6. 本轮交付状态

本轮仅新增安全联锁与审查文档，不变更 EEPROM 地址、通讯协议、历史记录枚举及 AFE 温度保护设定；没有进行真实硬件动作验证，不能视为量产完成。所有原有 C 源码保留原始 GBK 编码，避免无关编码变更和 Keil 工程兼容性问题。下阶段优先 **Keil ARMCC5 编译、板上 95°C/80°C 热保护与 NTC 开短路注入、STOP 唤醒安全关断测试**，通过后再考虑合并。
