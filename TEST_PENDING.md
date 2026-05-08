# 待测试清单

本文记录当前 SOC 重写后需要持续验证的事项。主机侧回放覆盖算法边界，Keil 和上板项仍需在目标环境完成。

## 项目级审查后待验证项 2026-05-09

- [ ] Windows + Keil MDK 完整编译：分别编译 `FD_Release` / `FD_Debug`，确认 ARMCC 下没有新增 warning/error。
- [ ] RS485 `0x03` 正常读兼容：覆盖 `0xD000 / 0xD100 / 0xD200 / 0xC000 / 0xC001 / 0xC002 / 0xC008 / 0x2000~0x2400`。
- [ ] RS485 `0x03` 异常读：非法地址、`reg_count=0`、跨区读取、超长读取应返回负响应，不允许越界取数。
- [ ] AFE MTP 写入：验证 `SH367309_SC_DelayT_Set()` 写 MTP `0x0E` 成功路径、失败路径和 `OtherElement.u16CBC_DelayT` 回退逻辑。
- [ ] 热管理超时：打开加热后持续低温场景应在 3 小时超时触发 `ERROR_HEAT`，关闭/重启后计数器应重新开始。
- [ ] 热管理与保护链路：任何充电相关保护触发后，`Heat_Cool` 不得重新打开 CHG MOS。
- [ ] Release 看门狗：`PROJECT_CFG_WDOG_ENABLE=1` 时确认 IWDG 正常启动、主循环喂狗、异常卡死可复位。
- [ ] 文档链接巡检：根目录 Markdown 不应再出现旧 Windows 绝对路径、副本工程路径或已过期看门狗描述。

## SOC 主机侧检查

- [x] 主机回放：执行 `python3 tools/soc_replay_test.py`，覆盖冷启动、快照恢复、设置一次 SOC、积分、循环/SOH、满电确认、低压表、RTC 静置、显示平滑、Fixed/Zero 覆盖和自动校准步长。
- [x] 语法检查：对 `SOC.c` 和 `SocEnhance.c` 执行 C99 `clang -fsyntax-only`，确认当前改动可被主机编译器解析。
- [x] Flash 兼容：保持 `STORAGE_FLASH_SOC_DATA` V2 结构、`StorageFlash_LoadSocData()`、`StorageFlash_SaveSocData()` 和旧 V1 快照迁移入口不变。
- [x] 通信兼容：保持 `InitData_SOC()`、`App_SOC()`、`SOC_UpdateSampleData()`、`SOC_PublishReportData()`、`SOC_ApplyRtcRelaxationCompensation()`、`SOC_ResetStoredSnapshotToDefault()` 调用签名不变。

## Keil 编译验证

- [ ] Keil 工程完整编译：使用 `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx` 的 `FD_Debug` / `FD_Release` 在 Windows + Keil MDK 环境编译。
- [ ] 确认以下文件与新 SOC 接口编译兼容：`SOC.c`、`SocEnhance.c`、`Flash.c`、`Sci_Upper.c`、`Can_HDX.c`、`LedBar.c`、`rtc_sleep.c`。
- [ ] 检查 map/链接结果，确认未新增 Flash 地址，未引入异常 RAM 增长。

## 启动与快照验证

- [ ] 无 SOC 快照且电压有效：擦除 SOC journal 后上电，确认按 OCV 表初始化 SOC，而不是固定 `60%`。
- [ ] 无 SOC 快照且电压无效：确认默认启动 SOC 为 `60%`，容量为 `OtherElement.u16Soc_Ah` 对应值。
- [ ] V2 快照恢复：确认 `soc/cap_now/cap_full/cycle_x100/dsg_acc` 可跨重启恢复。
- [ ] 旧 V1 快照兼容：写入旧格式 `SocNow/DsgSocInt/CycleTimes`，确认启动可读取并在下一次保存迁移为 V2。
- [ ] CRC 错误回退：破坏 SOC journal CRC，确认加载默认状态并在后续保存恢复 journal。
- [ ] Flash 保存失败重试：模拟首次保存失败，确认后续调度仍会重试。

## 积分与 SOH 验证

- [ ] 小电流死区：`Ichg/Idsg <0.4A` 时不积分。
- [ ] 样本去重：AFE 电流样本序号不变时，`App_SOC()` 不重复积分旧电流。
- [ ] 充放电方向切换：方向切换时积分余量不反向继承。
- [ ] 放电循环累计：每累计出厂容量 `1%` 的放电量，内部 `cycle_x100 + 1`。
- [ ] SOH 映射：确认 `SOH = clamp(100 - cycle / 50, 70, 100)`，并用 SOH 反推 `cap_full`。
- [ ] 设置一次 SOC：上位机 `0x1005` 后，内部 SOC、`CapNow` 和对外显示同步到设定值。
- [ ] `SOC_Fixed` / `SOC_Zero`：只覆盖对外显示，不破坏内部容量、循环和 Flash 快照。

## 满电验证

- [ ] 满电确认：非 `DSG`、`VCellMax >= max(V100,4180mV)` 后，满足 `VCellMin >=4150mV` 且压差 `<=150mV` 持续 `5s`，或内部 SOC `>=95%`、`VCellMin >=4100mV`、压差 `<=200mV` 累计 `15s` 后，每次只上修 `1%` 直到 `100%`。
- [ ] 满电前显示：未确认满电前内部和显示 SOC 最高不超过 `99%`。
- [ ] 停充满电：充电器停止输出、电流变成 0 且进入 `RELAX` 后，只要高压条件仍满足，也能确认 `100%`。
- [ ] 满电计数抗抖：满电条件短暂丢失时计数器递减而不是完全清零，确认采样抖动不会导致永远不到 `100%`。
- [ ] 压差异常：压差超过当前确认阈值时不确认满电。

## 低压与 OCV 验证

- [ ] 自动校准步长：满电确认、低压表、静置/RTC OCV、极端低压兜底，每次内部 SOC 校准变化都不得超过 `1%`。
- [ ] 宽范围低压表：非充电下 `VCellMin <=3400mV` 后，按 RELAX、轻载 `<=C/5`、中载 `<=C/2`、重载 `>C/2` 四档表驱动下修。
- [ ] 大电流 holdoff：`Idsg > C/2` 或刚结束大电流后的 `30s` 内，若 `VCellMin > V0 + 50mV`，低压表、low guard、静置 OCV 都不得校准 SOC。
- [ ] 显示空电区：`VCellMin <=3000mV` 时，所有电流档位都按表向 `0%` 收敛，但每次仍只允许下修 `1%`。
- [ ] 控制器保护前归零：`VCellMin <=2950mV` 时以最快 `1%/200ms` 收敛到 `0%`，不得一次跳变到 `0%`。
- [ ] 低压停放电：低压保护后电流变成 0 且仍低于 `3000mV` 时，`RELAX` 状态继续把 SOC 收敛到 `0%`。
- [ ] 极端兜底：`VCellMin <=2750mV/2500mV` 保留最快 `1%/200ms` 收敛，不作为正常 e-bike 空电依赖点。
- [ ] 重载补偿：大电流骑行 voltage sag 下，表目标应高于轻载目标，避免 SOC 过早掉到 0。
- [ ] 静置 OCV：运行态 `RELAX >=30min` 后只小步修正内部 SOC，显示继续平滑。
- [ ] RTC OCV：长时间休眠唤醒后按 OCV 小步修正，不产生大幅显示跳变。

## 异常输入验证

- [ ] 电压无效：模拟 `VCellMax < VCellMin`、单体低于 `2000mV`、单体高于 `5000mV`、压差高于 `1000mV`，确认 OCV、满电确认、低压 guard 均不动作。
- [ ] 故障状态：注入三级保护故障、AFE1/AFE2/ADC/CBC/温度异常，确认电压类校准被屏蔽。
- [ ] 故障恢复：故障清除且采样恢复合法后，SOC 校准可恢复正常。

## 通信与显示一致性

- [ ] CAN/RS485 读取：确认 `g_stCellInfoReport.SocElement.u16Soc` 为显示 SOC，`u16Soh` 为 SOH，容量字段仍为 `Ah * 100`，循环次数为整数次。
- [ ] 上位机基础参数：`0x2318~0x231B` 写入后，容量、循环、满电端点、空电端点运行生效。
- [ ] SOC 表写入：`0x2200` 起 42 个寄存器写入后，运行态 OCV 表生效；重启不要求持久化。
- [ ] LED 显示：确认 LED 电量段与 CAN/RS485 显示 SOC 一致。
- [ ] 断电重启：骑行放电、充电末端、低压末端三个场景断电重启后，SOC 能从快照恢复。
