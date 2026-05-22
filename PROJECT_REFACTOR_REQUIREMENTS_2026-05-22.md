# 项目根本需求与重构路线 2026-05-22

## 根本目标

本项目不是单纯追求源码行数减少，而是把 BMS 固件整理成可维护、可验证、可裁剪的量产 App：

- 架构清晰：启动、运行调度、采样、保护、SOC、通信、低功耗、显示、存储、工厂测试分别有明确边界。
- 方便阅读管理：入口文件只表达主流程，业务模块只暴露必要接口，配置和产品差异集中到 `Project_Config.h` / `Project_BuildGuard.h`。
- 减小 Code/ROM/RAM：用 map 文件确认收益，优先删除未链接、未使用、重复实现和可编译期确定的功能。
- 保持量产安全：IAP 地址、App 地址、保护策略、低功耗唤醒、上位机关键寄存器规则不能靠记忆，必须写入脚本、配置或文档。

## 不可破坏约束

- IAP/Bootloader 固定在 `0x08000000`。
- 正常 App 固定在 `0x08004800`，scatter 文件为 `103 + 309/Project/Users/Objects/FD_Release.sct`。
- 量产配置必须保持 `PROJECT_CFG_BUILD_PROFILE 0`。
- SOC 注入测试、Flash 破坏性测试、调试打印和 Watch 入口必须通过编译配置隔离。
- 保护/告警逻辑不能因为减小 Code 直接删除；只有满足以下条件之一才能裁剪：
  - map 证明未被链接，删除只是清理源码；
  - 有等价 AFE 硬件保护或其他模块接管，并在文档中写明；
  - 通过 `Project_Config.h` 增加明确开关，默认保持量产安全行为。
- 上位机写参数能力不能用裸 `#if 0` 随手关闭；如果产品要禁写，必须成为配置项，并明确哪些寄存器仍允许写入。

## 目标架构

```text
main.c
  -> AppInit      启动初始化、EEPROM 默认值、AFE/CAN/ADC/TIM/RTC 初始化顺序
  -> Runtime      主循环调度，只表达任务顺序，不塞业务细节

Runtime
  -> AFE/DataDeal         AFE 采样、电压/温度/电流换算、自动零点
  -> Protection/Fault     保护判定、故障上升沿、故障记录
  -> SOC/SocEnhance       SOC 积分、OCV 修正、满空锚点、显示平滑
  -> Comm/Sci/Can         RS485/Modbus、CAN 帧、通信唤醒
  -> LowPower/RTC         STOP/DEEP_SLEEP、RTC alarm、唤醒恢复
  -> UI/LedBar            数码管/灯板/按键显示
  -> Storage/Flash        参数、日志、SOC/AFE 快照
  -> Factory/Test         工厂老化、测试固件入口
```

模块依赖方向只允许从上往下。驱动层不能反向调用业务策略；业务模块之间需要共享状态时，优先通过小接口，而不是继续扩张 `main.h`。

## 当前优先级

1. 先保住边界：`main.c` 薄入口、`AppInit` 启动、`Runtime` 调度、`MosStartup` MOS 上电策略已经拆出，后续不得回退。
2. 先裁剪明确无风险项：未链接函数、重复常量表、固定产品不需要的运行时表、禁用串口的变量和处理路径。
3. 再处理大模块：
   - `Sci_Upper.c`：拆 0x03/0x06/0x10，写寄存器能力改为配置化，不能裸 `#if 0`。
   - `Fault.c`：区分“软件保护判定”和“故障记录”。如果软件保护已不运行，要文档化原因；如果仍是量产保护链，必须保留或重构，不能直接删。
   - `SH367309_DataDeal.c`：AFE 参数刷新表和上位机写入能力按产品需求配置化。
   - `SOC.c` / `SocEnhance.c`：合并重复状态和中间变量，保留编译期化学体系选择。
   - `FactoryAging.c`：老化模式只保留首次出厂自动执行、进度保存、完成退出三类职责。
   - `Can_HDX.c` / `CanFeidaoFrames.c`：继续拆分运行时状态、帧定义和低功耗唤醒。
4. 最后治理头文件：逐步减少 `main.h` 聚合 include，每个模块公开自己的最小 `.h`。

## Code/ROM 优化方法

每一轮优化必须记录：

- 优化前后 `FD_Release.map` 的 `Program Size` 和 `Total ROM Size`。
- 最大 Thumb Code 函数列表，确认下一刀来自真实热点。
- 最大 RO-data/RW-data/ZI-data 符号列表，确认表、缓存、运行时状态是否必要。
- 功能风险等级：
  - P0：地址、IAP、低功耗唤醒、保护链路、参数持久化。
  - P1：上位机读写、SOC、CAN 广播、老化模式。
  - P2：显示、日志、调试/测试辅助。

只有 P2 或 map 证明未链接的内容可以直接删；P0/P1 必须配置化或先写明替代路径。

## 当前未提交改动判定

当前工作区存在一组本轮开始前已有改动：

- `Fault.c/Fault.h`：大幅删除软件保护检测代码。这会改变保护/告警行为，不能按“无影响减码”直接提交，除非确认软件保护链路已由 AFE 硬件或其他模块接管。
- `Sci_Upper.c`：`0x06/0x10` 写寄存器处理被 `#if 0` 关闭。这能显著减小 Code，但会关闭上位机写参数/控制能力，应改成配置项和白名单，而不是裸关闭。
- `DataDeal.c`：注释掉电压默认 K/B 校准计算。若产品确认不需要 AFE 电压 K/B 校准，可保留并进一步清理校准入口；否则必须恢复。
- `RTC.c`：注释掉 RTC 唤醒周期受 IWDG 安全窗口限制的逻辑。低功耗加看门狗时存在唤醒超时风险，需单独验证。

这些改动可以作为减码方向，但必须进入配置化和验证流程后再合入。

## 每轮提交准入

- 构建：`FD_Release` 必须 0 error。warning 要么为 0，要么在文档中说明来源。
- 自检：`py -3 tools/project_check.py -q` 必须通过。
- 文档：大改动必须更新本文件或对应模块文档。
- Git：提交信息必须说明“改了什么、为什么、体积/风险结果”。

## SCI 写寄存器策略

`PROJECT_CFG_HOST_WRITE_ENABLE` 统一控制 Modbus `0x06/0x10` 写寄存器入口：

- `0`：量产默认，减小 Code；写请求返回 `RS485_ERROR_NO_PERMISSION`，不允许静默成功。
- `1`：服务/调试固件可打开，恢复参数写入、SOC 表写入、RTC 写入、SN 写入和升级握手等路径。

后续若只允许部分写操作，应继续拆分白名单，而不是回到裸 `#if 0`。

本策略在干净 worktree 的 `FD_Release` 对比：

```text
开启写入口基线: Code=49640 RO-data=2568 RW-data=896 ZI-data=5416, ROM/bin=52456
关闭写入口结果: Code=46620 RO-data=2568 RW-data=896 ZI-data=5416, ROM/bin=49436
净减少: Code/ROM/bin 3020 bytes
```

关闭后 `Sci_Deal_WrReg_0x06()` 和 `Sci_Deal_WrRegs_0x10()` 仍保留 14 字节入口用于返回 no-permission；具体写处理函数由链接器移除。
