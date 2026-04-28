# 主流 BMS SOC 策略对比与本工程取舍

本文用于说明当前 SOC 模块为什么选择“安时积分 + 可信端点/OCV 校准 + 对外平滑发布”的中等增强方案。目标排序为：用户体验第一，架构清晰稳定第二，精度提升第三。

## 1. 结论

当前工程是保护板 BMS 场景，不是专用 fuel gauge 芯片。最合适的策略不是单纯电压查表，也不是一次性引入复杂 `EKF`、完整 impedance table 或大规模参数体系，而是：

1. 运行中用 coulomb counting 跟踪容量变化。
2. 只在可信条件下使用满电、空电、静置 `OCV` 做校准。
3. 内部 SOC 允许快速收敛，对外 `DisplaySoc` 平滑发布。
4. `Flash` 快照保存足够恢复体验和学习状态，但不把每个小数积分都写入 Flash。
5. `FCC/SOH` 学习只在可信满/空锚点之间进行，并限制学习跨度、范围和单次变化。

这套方案牺牲一部分理论最高精度，换取保护板产品更重要的稳定读数、可解释行为、低维护成本和低 Flash 写入压力。

## 2. 主流方案对比

| 策略 | 典型做法 | 优点 | 风险 | 对本工程的取舍 |
| --- | --- | --- | --- | --- |
| 电压/OCV 查表 | 静置后按单体电压查 SOC 表 | 简单、资源占用低 | 负载下误差大，`LFP` 平台区分辨率差，容易跳变 | 只用于冷启动、RTC 唤醒、长静置小幅校准 |
| 安时积分 | 按电流对时间积分，跟踪 charge in/out | 运行态连续、用户读数稳定 | 依赖初始 SOC、零点、电流采样、`FCC` | 作为主路径，结合端点校准修正漂移 |
| 电压 + IR 修正 | 用电流和内阻估算负载压降，再回推 OCV | 比纯电压好 | 需要温度/老化/内阻参数，参数维护成本高 | 当前不引入完整 IR table，只保留端点和弱单体保护 |
| Impedance Track / 高级 fuel gauge | 结合 OCV、Qmax、阻抗、负载、温度模型 | 精度最高 | 算法和参数体系复杂，对数据采集要求高 | 不在保护板 MCU 内复刻；若后续要最高精度，建议外接专用 gauge |
| ModelGauge 类算法 | 估算负载下 OCV 并融合模型 | 用户体验好、无需简单纯积分 | 依赖芯片/模型能力 | 借鉴“融合与平滑”思想，不移植复杂模型 |

## 3. 资料依据

- [TI BQ769x2 FAQ](https://www.ti.com/lit/pdf/sluaaq5)：`BQ769x2` 是监控/保护芯片，本身不集成完整 gauging，但提供电压、电流、累计电荷、温度等 MCU 计算 SOC/SOH 所需数据。
- [TI Battery Gauging Algorithm Comparison](https://www.ti.com/lit/pdf/sluaar3)：对比 voltage correlation、voltage + IR correction、coulomb counting 与 Impedance Track，指出纯电压简单但受负载影响，纯积分依赖初始容量和满充容量。
- [TI Impedance Track](https://www.ti.com/lit/an/slua364b/slua364b.pdf)：高级 gauge 会同时使用化学容量、阻抗、负载、温度等信息，并有 charge/discharge/relax 模式切换。
- [ST AN3395](https://www.st.com/resource/en/application_note/an3395-sensing-resistor-selection-and-usage--in-stc310x-battery-monitoring--applications-stmicroelectronics.pdf)：电压测量与 coulomb counting 是 gas gauge 常见组合，轻载/静置用 OCV 更新，重载用积分跟踪。
- [ADI ModelGauge](https://www.analog.com/en/resources/technical-articles/get-enhanced-safety-accurate-stateofcharge-and-longer-runtime-for-your-portable-device-battery.html)：强调在负载下估算 OCV 并融合模型，说明用户可见 SOC 不能只依赖瞬时端电压。

## 4. 本工程策略映射

| 目标 | 实现点 | 用户体验收益 |
| --- | --- | --- |
| 避免 SOC 抖动 | `CHG / DSG / RELAX` 模式滞回，小电流死区不积分，连续约 5s 无有效电流才进入静置 | 插拔负载、采样噪声不会导致 SOC 方向反复变化 |
| 避免满电虚高 | 满电需要充电模式、单体满足满电窗口、taper 电流低于 `C/20` 且持续约 60s | 充电早期高电压不会立即显示 100% |
| 避免低压风险被高 SOC 掩盖 | 弱单体进入 `V0 + 30mV` 或 `V0` 附近时加速下修 | 用户在低压临界区看到更保守、更安全的 SOC |
| 避免显示跳变 | 内部 SOC 与对外 `DisplaySoc` 分离，正常约 `1%/s` 跟随 | 数码管、CAN、RS485 读数更稳定 |
| 保持冷启动可信 | `Flash` SOC 快照 V2 保存 `SOC/DSG/FCC/CapNow/learning`，兼容 V1 | 升级后不丢现场 SOC，旧快照可迁移 |
| 降低 SOH 漂移 | 满/空可信锚点之间统计 passed charge，限制学习跨度、容量范围和单次变化 | 长期使用后容量基准可慢慢贴近真实包体 |

## 5. 架构边界

| 模块 | 边界 |
| --- | --- |
| `SOC.c` | 保持应用调度入口，只加载配置、200ms 调度、传入采样输入 |
| `SocEnhance.c` | 维护 SOC runtime/context，负责状态机、积分、校准、显示平滑、快照触发 |
| `Flash.c/.h` | 只负责 SOC 快照 V1/V2 的读写兼容和 journal 存储 |
| 文档 | 解释策略、测试和维护边界，避免把业务说明塞进源码注释 |

## 6. 不做的事

当前版本明确不做以下扩展：

1. 不引入动态内存、后台任务或跨模块隐式依赖。
2. 不新增 CAN/RS485 地址，不改变 `InitData_SOC()`、`App_SOC()`、`SOC_ApplyRtcRelaxationCompensation()` 签名。
3. 不引入完整 `EKF`、impedance table、温度容量矩阵或大规模参数表。
4. 不把 `SOC_Table_Set` 的自定义 OCV 表跨重启持久化；这仍是后续独立需求。

## 7. 后续升级方向

若后续产品需要进一步提高精度，建议按风险从低到高推进：

1. 增加实验台 SOC 场景回放脚本，先验证现有策略。
2. 补充温度分档的端点阈值修正，但仍保持有限状态。
3. 持久化自定义 OCV 表，解决上位机写表断电丢失问题。
4. 若必须达到专用 fuel gauge 精度，再评估外接 gauge 或独立算法芯片，不建议在当前保护板主控内直接堆复杂模型。
