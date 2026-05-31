# Flash / IAP 地址门禁执行方案

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.sct`, `103 + 309/Project/Source/Flash.h`, `103 + 309/Project/Source/Flash.c`, `103 + 309/Project/Source/conf/Project_Config.h`, `tools/project_check.py`, `tools/soc_flash_app_safe.ps1`
最后更新时间：2026-05-31
未确认事项：量产 MCU 实际 Flash 容量、App 最大结束地址、IAP 固件是否固定读取 SRAM mailbox `0x20004FE0`、Release map 缺失时是否允许发布。

## 1. 目标

本方案只定义后续门禁和确认项，不修改源码、scatter、烧录脚本或协议行为。

S1 阶段目标是把 App/IAP/Flash 存储/mailbox 的高风险边界变成可检查规则：

1. IAP/Bootloader 固定在 `0x08000000`。
2. App 固定从 `0x08004800` 启动。
3. App 链接结束地址不得覆盖 `0x0801C000` 以后持久化存储区。
4. App->IAP SRAM mailbox `0x20004FE0` 不得被普通 RW/ZI 数据占用。
5. 使用 `0x0801C000+` 后 64K Flash 存储前，必须证明量产 MCU Flash 容量满足要求。
6. 安全烧录继续只允许 App 写入 `0x08004800`，禁止裸写 App bin 到 `0x08000000`。

## 2. 当前源码事实

| ID | 当前事实 | 证据 | 判断 |
|---|---|---|---|
| FACT-S1-IAP-001 | IAP 起始地址为 `0x08000000`，App 起始地址为 `0x08004800` | `Flash.h` 定义 `FLASH_ADDR_IAP_START`, `FLASH_ADDR_APP_START`; `tools/soc_flash_app_safe.ps1` 拒绝非 `0x08004800` 地址 | MUST_KEEP |
| FACT-S1-FLASH-001 | 持久化存储页从 `0x0801C000` 开始 | `Flash.h` 定义 AFE/RW/LOG/SOC/upgrade/aging/legacy flag 地址 | MUST_KEEP，但需要容量和链接边界门禁 |
| FACT-S1-SCT-001 | 当前 scatter 写法允许 `LR_IROM1 0x08004800 0x00020000`，结束地址理论上到 `0x08024800` | `CommomSH367309_16series_103RCT6_C.sct` | 高风险，需要确认并收紧 |
| FACT-S1-SCT-002 | 当前 `RW_IRAM1 0x20000000 0x00005000` 覆盖到 `0x20005000` | `CommomSH367309_16series_103RCT6_C.sct` | 高风险，尾部包含 mailbox 区间 |
| FACT-S1-MAILBOX-001 | App->IAP mailbox 地址为 `0x20004FE0` | `Flash.c` 中 `APP_UPGRADE_MAILBOX_ADDR` | MUST_KEEP 或需 IAP 双端协议确认后才可变更 |
| FACT-S1-CHECK-001 | `tools/project_check.py` 当前只检查 Release map 的 `LR_IROM1/ER_IROM1` 起始地址和 ROM/RAM size | `check_release_map()` | 不足，需要补结束地址、存储重叠和 mailbox 保留检查 |
| FACT-S1-MAP-001 | 当前本机缺少 `FD_Release.map` 时，脚本只给 warning | `check_release_map()` | 文档/开发阶段可 warning；发布阶段应 fail |

## 3. 当前缺口

| 缺口 | 影响 | 当前处理 |
|---|---|---|
| App 结束地址未被门禁限制到 `0x0801C000` 之前 | App 体积增长后可能覆盖 AFE/RW/LOG/SOC/升级/老化存储页 | 先文档确认，后续再改 `tools/project_check.py` |
| `RW_IRAM1` 未保留 `0x20004FE0` mailbox | mailbox 可能被 RW/ZI 分配或运行时写坏，导致进入 IAP 不稳定 | 先确认 IAP 固件地址是否固定，再决定 scatter 保留方式 |
| Release map 缺失仍可继续通过快速检查 | 无法证明最终链接基址、结束地址和 RAM 布局 | 快速检查保留 warning；发布/严格模式应 fail |
| `0x0801C000+` 存储依赖 Flash 容量 | 若量产芯片不足 128KB，保存路径可能越界 | 需要 BOM/芯片读数/构建目标共同确认 |
| 旧文档存在升级清参策略版本不一致 | 容易误判升级后会保留或清除哪些参数 | 已在本轮同步校正设计文档 |

## 4. 后续门禁规则草案

这些规则是待确认的执行方案，不代表本轮已经修改脚本。

| Gate ID | 门禁规则 | 检查来源 | 建议失败策略 |
|---|---|---|---|
| G-S1-001 | `LR_IROM1` base 必须等于 `0x08004800` | `FD_Release.map` | fail |
| G-S1-002 | `ER_IROM1` exec base 必须等于 `0x08004800` | `FD_Release.map` | fail |
| G-S1-003 | App load/exec 结束地址必须 `<= 0x0801C000`，除非用户确认重划存储区 | `FD_Release.map` 或 scatter/bin size | fail |
| G-S1-004 | `RW_IRAM1` 不得覆盖 `[0x20004FE0, 0x20005000)`，或 scatter 必须显式 reserve mailbox | scatter + map | fail |
| G-S1-005 | Release map 缺失时，开发快速检查 warning，发布/严格模式 fail | `tools/project_check.py` 参数模式 | strict fail |
| G-S1-006 | `tools/soc_flash_app_safe.ps1` 必须继续拒绝非 `0x08004800` 地址 | 脚本静态检查 + dry-run | fail |
| G-S1-007 | 使用 `0x0801C000+` 存储前必须证明 Flash 容量至少覆盖到 `0x08020000` | BOM、芯片寄存器、ST-Link 或运行日志 | release fail |

## 5. 需要用户确认的需求表

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| REQ-S1-FLASH-END | App 链接结束地址必须小于或等于 `0x0801C000` | scatter 当前给 `0x08004800 + 0x00020000`; `Flash.h` 从 `0x0801C000` 开始定义存储页 | 当前缺硬门禁 | App 变大后覆盖参数/SOC/日志 | CHANGE_NEEDED | 后 16KB 存储区是否固定保留？App 最大结束地址是否定为 `0x0801C000`？ | 固定为 `<= 0x0801C000`，先补 map 门禁，再改 scatter | 待确认 |
| REQ-S1-MAILBOX | SRAM mailbox `0x20004FE0` 必须被 App 保留 | `Flash.c` 使用 `APP_UPGRADE_MAILBOX_ADDR`; scatter 当前 RAM 覆盖到 `0x20005000` | mailbox 落在普通 RAM 尾部 | IAP 请求可能被覆盖 | CHANGE_NEEDED | IAP 固件是否固定读取 `0x20004FE0`？是否允许调整 mailbox 地址？ | 不改协议地址，优先在 scatter 保留尾部 RAM | 待确认 |
| REQ-S1-MAP | 发布前必须有可解析 `FD_Release.map` | `tools/project_check.py` 当前缺 map 只 warning | 无 map 仍可继续快速检查 | 无法证明最终链接布局 | MUST_KEEP | 缺 Release map 时，是否禁止发布？ | 开发 warning，发布/严格模式 fail | 待确认 |
| REQ-S1-MCU-FLASH | 使用后 64K 存储前必须确认量产 MCU Flash 容量 | `Flash.h` 使用 `0x0801C000+`; `Flash.c` 有 Flash size register 读取 | 文档未固定 BOM 结论 | 64KB 芯片上越界写 | UNKNOWN | 量产 MCU 是 128KB 以上，还是存在 C8 大小差异批次？ | 发布前用 BOM + 芯片读数双证据确认 | 待确认 |
| REQ-S1-POLICY | 升级清参策略必须按当前 `Project_Config.h` 描述 | `Project_Config.h` 当前 policy version `0x0005`，多项 reset 为 1 | 旧文档曾写 `0x0004` 且保留多项参数 | 升级后参数保留/清除预期错误 | CHANGE_NEEDED | 当前 `0x0005` 清参范围是否就是下一版量产策略？ | 先按源码修文档；发布前确认策略 | 待确认 |

## 6. 分阶段执行计划

| 阶段 | 文件范围 | 动作 | 禁止改动 | 验证 | 回滚 |
|---|---|---|---|---|---|
| S1-D0 | `docs/review/*`, `docs/design/storage_design.md`, `docs/README.md` | 建立本方案，修正文档中已过期的升级清参策略描述 | 禁止改 `.c/.h`、scatter、Keil 工程、脚本行为 | `python3 tools/project_check.py -q`, `git diff --check` | 回滚本轮文档 patch |
| S1-D1 | `tools/project_check.py` | 用户确认后补 map 结束地址、mailbox、strict/release 模式检查 | 禁止改 Flash 地址和协议 | 本机脚本检查 + 缺 map/模拟 map 用例 | 回滚脚本 patch |
| S1-D2 | scatter 文件 | 用户确认后收紧 App Flash 长度并 reserve SRAM mailbox | 禁止改 App 起始地址和 mailbox 协议 | Keil build 生成 map，确认 `LR/ER/RW` | 回滚 scatter patch |
| S1-D3 | Keil 构建产物 | Windows/Keil 真构建，证明 `FD_Release.map/bin` 满足门禁 | 禁止绕过安全脚本烧录 | map/bin/vector 检查 | 不发布该产物 |
| S1-D4 | 实机通信 | 只在用户确认后做 COM4/CAN/ST-Link 验证 | 禁止裸写 `0x08000000` | `0xD000/0xD300`, IAP 入口 smoke, 存储读写 smoke | 重新烧录上一版 App |

## 7. 当前验证边界

本轮只做到源码和文档层面的证据同步：

- 未运行 Keil/ARMCC 真构建。
- 未生成新的 `FD_Release.map`。
- 未连接 ST-Link、COM4、CAN 或真实 BMS 板。
- 未修改 `tools/project_check.py`、scatter 或烧录脚本。
- 未证明量产 MCU Flash 容量。

因此，本轮结论不能当作“发布已安全”，只能作为下一步门禁实现和用户确认的输入。

## 8. 下一步建议

1. 先由用户确认 `REQ-S1-FLASH-END`, `REQ-S1-MAILBOX`, `REQ-S1-MAP`, `REQ-S1-MCU-FLASH`。
2. 确认后只改 `tools/project_check.py`，先做脚本门禁，不动 scatter。
3. 脚本门禁可验证后，再单独处理 scatter 的 App 长度和 mailbox reserve。
4. scatter 修改必须用 Keil 真构建和 map 证明，不用静态推断替代。
