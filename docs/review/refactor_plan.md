# 103-309 BMS 后续分阶段重构计划

> 文档状态：CURRENT
> 源码验证：PARTIAL
> 主要参考源码：`main.c`, `AppInit.c`, `Runtime.c`, `System_Init.c`, `DataDeal.c`, `Flash.c`, `EEPROM.c`, `System_Monitor.c`, `Can_HDX.c`, `Sci_Upper.c`, `SocEnhance.c`, `conf/Project_Config.h`
> 最后更新时间：2026-05-31
> 前提：未完成需求确认前，不进入源码重构。所有阶段都必须可回滚、可验证、保护上位机协议、安全逻辑和硬件行为。

## 轻量重构原则

后续整体架构优化必须优先保持代码清晰、简单、直接，方便人工阅读和上板排查。允许的优先动作是删除无用代码、删除重复代码、删除不必要变量、减少裸全局变量、简化数据流和控制流；模块边界要清楚，但分层必须轻量。

禁止为了“架构感”引入复杂框架、多层 wrapper、深层嵌套或大而全抽象。任何简化都不能影响功能、协议兼容、硬件行为、SOC、保护、低功耗和 IWDG；涉及这些边界时，必须先拆成小批次并补验证。

## 当前阶段执行门禁

当前阶段是“需求澄清 + 文档/风险闭环 + 可执行计划”。除非用户明确批准，禁止修改 `.c/.h`、Keil 工程、编译宏、协议行为、Flash 布局、CAN ID、Modbus 寄存器、上位机数据含义或烧录脚本。

| 阶段 | 目标文件 | 任务 | 禁止修改 | 风险 | 前置条件 | 验证 | 回滚 |
|---|---|---|---|---|---|---|---|
| S0 | `docs/review/*`, `docs/README.md`, `README.md` | 修正 source-first 当前事实、标记 stale 文档冲突、收敛需求确认表和阅读入口 | 禁止改源码/工程/协议/脚本 | 低 | 当前源码复核完成 | `python3 tools/project_check.py -q`, `git diff --check` | revert 文档 commit |
| S1 | `tools/project_check.py`, `docs/design/bootloader_iap_design.md`, `docs/review/*` | 增加 App 结束地址、后 16KB 存储区、IAP mailbox 的只读门禁设计；先文档后脚本 | 禁止改 scatter 和 Flash 地址 | 高 | 用户确认 App 最大边界、真实 MCU 容量和 IAP mailbox 规则 | project check 能识别 map 缺失/地址越界；Windows/Keil 生成 map 后复核 | revert 脚本/文档 commit |
| S2 | `SH367309_DataDeal.c`, `Sci_Upper.c`, 协议文档 | AFE 参数写入范围校验和错误码策略 | 禁止改 AFE MTP 时序、参数地址、寄存器含义 | 高 | 用户确认写权限和非法值返回策略 | Keil 编译；上位机写合法/非法 AFE 参数；MTP 写回验证 | 单独 revert |
| S3 | `I2C_AFE1.c`, `SH367309_Func.c`, `System_Monitor.c`, AFE 文档 | AFE 失败可见性与 fail-safe 策略 | 禁止直接改变 MOS 安全动作，除非策略确认 | 高 | 用户确认连续失败阈值、AFE watchdog 和安全动作 | AFE 断线/CRC/I2C 失败实测，`0xD000`/fault/CAN 状态一致 | 单独 revert |
| S4 | `SleepDeal.c`, `conf.c`, `rtc_sleep.c`, `Can_HDX.c`, 低功耗文档 | wake source matrix 与通信活跃判定 | 禁止改 BKP flag 含义、唤醒源电平、CAN ID | 高 | 用户确认哪些唤醒源可进入正常运行 | STOP/reset-sleep、UART/CMNT/CHG/key/CAN 唤醒实测 | 低功耗单独分支 revert |
| S5 | `Sci_Upper.c`, `docs/protocol/*`, 上位机测试脚本 | 空实现/`#if 0` 写入口返回语义 | 禁止改地址、长度、正常读窗口、`0xC002`、`0xD300` | 中高 | 用户确认废弃/占位/恢复列表 | Modbus 0x03/0x06/0x10 回归，上位机回归 | 单独 revert |
| S6 | `SocEnhance.c`, `SOC.c`, `tools/soc_*`, SOC 文档 | SOC 校准阻断和满电锚点策略；当前真实电流主路径已生效，只处理虚拟电流调试入口隔离 | 禁止无测试改 OCV/full/empty/rest/smoothing | 高 | 用户确认体验优先还是安全优先、虚拟电流是否保留 | host replay、充放电/静置/RTC 实测、`0xD000`/LED/CAN 一致 | 算法参数与代码分开 revert |
| S7 | `Flash.c`, `System_Monitor.c`, `DataDeal.c`, storage 文档 | 存储失败可见性、低功耗阻塞语义和升级清参策略 | 禁止改 Flash 地址、数据结构、magic/version/CRC | 高 | 用户确认存储失败处置策略和 policy `0x0005` 清参范围 | 写失败注入、断电恢复、`ERROR_STATUS_EEPROM_STORE` 可见，升级前后参数矩阵 | 单独 revert |
| S8 | 已确认死代码相关文件 | 第一批净删减：`App_WarnCtrl`、注释旧实现、空函数、未用测试入口 | 禁止删客户可见协议、保护、MOS、低功耗、IAP、老化 | 中 | 用户逐项确认删除清单 | Keil 编译、project_check、协议 smoke test | 每类删除单独 commit |

## 阶段 1：需求确认阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 只改 `docs/review/*` 和权威文档，不改源码 |
| 不能改什么 | 不改 `.c/.h`、Keil 工程、编译宏、协议行为 |
| 验证方法 | 确认 `requirement_questions.md` 中 P0/P1 项逐条有用户决定 |
| 需要确认 | 虚拟电流调试入口、均衡、Flash 容量/IAP 地址、Host 写权限、老化、低功耗策略 |
| 回滚方式 | 删除或恢复新增文档即可 |

## 阶段 2：文档补全阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 建立 `docs/README.md`, `docs/project_overview.md`, `docs/architecture.md`, `docs/module_map.md`, `docs/design/*`, `docs/test/*` |
| 不能改什么 | 不移动/删除旧文档，除非后续确认 |
| 验证方法 | 每份权威文档开头标注源码验证状态、参考源码、未确认事项 |
| 需要确认 | 哪些历史文档可以归档，哪些客户需求仍有效 |
| 回滚方式 | 保留旧文档，删除新权威文档即可回到原状态 |

## 阶段 3：模块边界整理阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 先只新增接口设计文档；代码阶段优先净删减和移动已有边界清楚的逻辑，不为“完整架构”新增无收益 helper/wrapper |
| 不能改什么 | 不改采样周期、不改 `g_stCellInfoReport` 字段布局、不改协议地址 |
| 验证方法 | 编译、Modbus `0xD000/0xC002/0xD300` 回归、CAN 周期帧抓包 |
| 需要确认 | 是否允许把客户逻辑从 `DataDeal.c` 拆到独立模块 |
| 回滚方式 | 每个边界单独 commit；不改变行为时可直接撤回该 commit |

## 阶段 4：Flash / EEPROM / 参数存储阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 固化 Flash 布局文档、增加地址/容量检查脚本、明确 EEPROM 兼容层命名 |
| 不能改什么 | 不改存储地址、不改 record 格式、不改写入策略，除非确认硬件容量 |
| 验证方法 | 后 64K 快速测试、掉电恢复测试、参数写失败回滚测试、map 地址检查 |
| 需要确认 | 真实 Flash 容量、工厂是否允许后 64K 存储、升级参数策略 |
| 回滚方式 | 脚本和文档可单独回滚；存储格式变更必须另立迁移阶段 |

## 阶段 5：通信协议适配层阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 先生成权威 Modbus/CAN 协议表；后续代码只抽读写副作用表 |
| 不能改什么 | 不改地址、字段顺序、异常码、CAN ID、`0xC002`、`0x14F80208` |
| 验证方法 | 上位机实时监控、产品信息读取、老化时间读取、CAN App read/write/block read |
| 需要确认 | Host 写权限、CAN service 命令集、老化控制是否保留 |
| 回滚方式 | 协议层改动必须小 commit，保留旧函数入口 |

## 阶段 6：ADC / AFE 数据流阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 保持当前 `DataLoad_Current()` 真实电流主路径，隔离或删除残留虚拟电流调试入口，梳理 AFE sample -> DataLoad -> SOC 顺序 |
| 不能改什么 | 不改 AFE 保护阈值、不改 MOS 初始策略、不改 I2C 时序 |
| 验证方法 | AFE 电流方向/零点实测、SOC sample seq、CAN 电流、Modbus 电流一致性 |
| 需要确认 | `test_Autocurrent_cycle()` 归属、Type-C 电流是否计入 SOC |
| 回滚方式 | 独立 commit；问题时 revert 本阶段，不保留临时切换路径 |

## 阶段 7：SOC 阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 先补测试，再整理状态命名/输入输出边界 |
| 不能改什么 | 不改 1% 校准硬约束、不改满电/低压锚点、不改 `0xD300` 隔离 |
| 验证方法 | 主机回放、真实充放电、RTC rest、snapshot 断电恢复、上位机读取 |
| 需要确认 | 初始 SOC、OCV 表、Type-C 电流、测试模式策略 |
| 回滚方式 | 算法参数与结构调整分离，先保留旧字段 |

## 阶段 8：RTC 低功耗 / IWDG 阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 统一阻塞原因、RTC 周期策略、IWDG 安全窗口文档和测试 |
| 不能改什么 | 不改唤醒源电平、不改 AFE sleep、不改 IAP/Flash busy 阻塞 |
| 验证方法 | STOP 电流、唤醒源、通信恢复、IWDG 长稳、过放深睡 |
| 需要确认 | 休眠中是否周期 CAN 广播、最大休眠周期、fault 是否阻塞 sleep |
| 回滚方式 | 保留旧 `rtc_sleep()` 执行器，外层策略可回退 |

## 阶段 9：LED / 显示阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 明确显示模型：SOC、图标、故障、睡眠预览、按键 |
| 不能改什么 | 不改 GPIO 引脚、不改 TIM4 扫描频率、不改低功耗前引脚安全态 |
| 验证方法 | 各 SOC 值显示、按键显示窗口、长按休眠、STOP 前泄漏、故障显示 |
| 需要确认 | 充电图标语义、故障显示模式、长按时间 |
| 回滚方式 | 显示模型独立 commit，可回退到当前 `APP_LedBar()` |

## 阶段 10：最终回归测试阶段

| 项目 | 内容 |
|---|---|
| 修改范围 | 只做验证、文档更新、发布检查 |
| 不能改什么 | 不在回归阶段混入功能变更 |
| 验证方法 | 编译、Modbus、CAN、存储、SOC、ADC/AFE、保护、MOS、低功耗、IAP、LED、老化 |
| 需要确认 | 出货标准、客户协议版本、硬件测试清单 |
| 回滚方式 | 每阶段都有独立 commit 和文档记录，失败回滚到上一个已验证阶段 |
