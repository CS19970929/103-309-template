# 仓库协作规则



- 全部使用中文回答。
- 进行较大的代码变更时，需要同步生成说明文档，并自动创建描述清楚的 git 提交。
- 不要依赖单次对话记忆。涉及烧录地址、测试模式、量产隔离、上位机启动方式的规则必须写入仓库脚本或文档。
- 修改用户上位机代码后，必须立即编译最新 exe。CAN 用户上位机固定在 `BMS_CommTool_Upgrade_UI.exe` 基础上修改，生成文件必须覆盖 `dist\BMS_CommTool_Upgrade_UI.exe`，不要另起新的 exe 名称。
- 老化剩余时间必须在原上位机界面里单独可见，不能只写在日志或命令行输出；当前入口是 `其它功能 -> 常用功能 -> 读取老化时间`，通过 `0x13 BMS_AGING_STATUS` 解析 `0x14F80208` 广播。
- BMS 序列号、硬件版本、软件版本必须在原上位机 `实时监控` 界面最底部边栏显示，读取来源固定为 `0xC002` 的 48 个寄存器。

## 103 + 309 烧录安全规则

- IAP/Bootloader 地址是 `0x08000000`。
- 正常 App 地址是 `0x08004800`。
- App scatter 文件是 `103 + 309/Project/Users/Objects/FD_Release.sct`。
- 禁止把 `FD_Debug.bin` 或 `FD_Release.bin` 裸写到 `0x08000000`，否则会覆盖 IAP，板子可能表现为死机或无法进入正常程序。
- App 烧录必须优先使用安全脚本：
  `.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash`
- 修改或新增烧录脚本时，必须保留 `0x08004800` 地址检查和 dry-run 输出。

## SOC 测试模式隔离

- 量产程序必须保持 `PROJECT_CFG_BUILD_PROFILE 0`。
- SOC 测试固件才允许打开：
  `PROJECT_CFG_BUILD_PROFILE 2`
  `PROJECT_CFG_SOC_TEST_MODE_ENABLE 1`
  `PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX 300`
- 测试模式必须通过编译配置隔离，不能影响正常出货量产程序。
- 测试结束后要恢复量产配置，并通过 `COM4/19200/slave=1` 读取 `0xD000` 和 `0xD300` 确认板端运行状态。
- 量产固件读到 `0xD300 supported=0` 是正常结果，代表 MCU 注入式 SOC 测试入口已关闭。

## SOC 测试上位机

- 不要直接双击 `tools\soc_test_ui.py`，这可能使用错误的 Python 环境并报 `No module named 'serial'`。
- 固定启动方式：
  `.\tools\start_soc_test_ui.ps1`
- 自动演示启动方式：
  `.\tools\start_soc_test_ui.ps1 -Demo -Port COM4 -Baud 19200 -Slave 1 -Samples 10 -Interval 0.5`
- 上位机在线监控依赖 `pyserial`，当前机器可用环境是 Windows Python Launcher 的 `py`。
- 当前桌面可见环境的 `py` 默认可能指向 Python 3.12，固定脚本默认使用已验证的 `py -3.9`。
- 若串口打开失败，先检查是否有其他程序占用 COM 口，再确认 `py -3.9 -c "import serial"` 成功。

# AGENTS.md

## 项目背景

这是一个嵌入式 BMS 固件项目。

项目可能包含：

- STM32F0 / STM32F1 MCU 代码
- 标准外设库或寄存器级代码
- SOC 算法
- ADC 采样
- AFE 驱动
- CAN / Modbus / UART 通信
- Flash / EEPROM / 参数存储
- RTC 低功耗
- IWDG 独立看门狗
- LED / Charlieplexing 显示
- Bootloader / IAP

## 硬性规则

- 在需求经过用户确认之前，不要修改源码。
- 不要引入 HAL，除非用户明确批准。
- 不要引入 RTOS，除非用户明确批准。
- 不要使用 malloc，除非用户明确批准。
- 不要破坏现有 Modbus / CAN / 上位机协议兼容性。
- 不要修改客户可见的协议寄存器、帧格式、CAN ID 或数据含义，除非用户明确批准。
- 优先使用 STM32 标准外设库或寄存器级实现。
- 优先编写简单、清晰、可维护的 C 代码。
- 避免过度设计。
- 避免一次性大规模重构。
- 每个重要修改都必须同步更新文档。

## 必须遵守的工作流程

在修改代码之前，必须先完成：

1. 阅读并理解整个项目。
2. 从源码中提取当前已经实现的需求。
3. 从协议行为、配置、注释和文档中提取隐含需求。
4. 找出模糊、过时、冲突或可能被误解的需求。
5. 创建需求确认表。
6. 让用户逐条确认需求。
7. 在用户明确做出决定之前，不要修改源码。

## 文档要求

需要创建或更新：

- `docs/review/full_project_review.md`
- `docs/review/module_map.md`
- `docs/review/requirement_confirmation.md`
- `docs/review/requirement_questions.md`
- `docs/review/risk_list.md`
- `docs/review/refactor_plan.md`
- `docs/changelog/change_log.md`
- `docs/test/test_plan.md`

## 文档整理规则

在审查或更新文档时：

- 源码是第一可信来源。
- 现有文档只能作为参考。
- 不要默认旧文档是正确的。
- 如果文档和源码冲突，必须明确标记冲突。
- 未经用户确认，不要直接删除旧文档。
- 过期文档优先移动到 `docs/archive/` 归档。
- 重复文档需要合并为少量权威文档。
- 每份更新后的文档必须标记状态：
  - 已按源码验证
  - 部分验证
  - 未验证
  - 历史归档
- 每份文档更新时，必须说明参考了哪些源码文件。
- 文档整理阶段不要修改源码，除非用户明确要求。

## 需求确认分类

每条需求必须归类为以下类型之一：

- `MUST_KEEP`：必须保留，行为不能随便改变
- `KEEP_BUT_REFACTOR`：需求保留，但实现可以重构
- `CHANGE_NEEDED`：需求可能需要修改
- `MAYBE_UNUSED`：可能已经不再需要
- `MISUNDERSTOOD`：当前实现可能误解了需求
- `CONFLICT`：与其他需求或当前实现存在冲突
- `UNKNOWN`：无法从代码判断，必须询问用户
- `REMOVE_CANDIDATE`：可能可以删除，但必须由用户确认

## 输出规则

当需要向用户确认需求时，必须使用清晰的表格：

| 字段 | 说明 |
|---|---|
| Requirement ID | 需求 ID |
| Requirement description | 需求描述 |
| Evidence from code | 代码证据 |
| Current behavior | 当前行为 |
| Risk | 风险 |
| Codex judgment | Codex 判断 |
| Question for user | 需要用户确认的问题 |
| Suggested decision | 建议决策 |
| User decision placeholder | 用户决策占位 |

在用户确认之前，不要修改代码。
