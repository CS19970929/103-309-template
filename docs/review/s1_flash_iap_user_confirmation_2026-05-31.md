# S1 Flash / IAP 地址门禁用户确认包

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码/脚本/文档：`103 + 309/Project/Source/Flash.h`, `103 + 309/Project/Source/Flash.c`, `103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.sct`, `tools/project_check.py`, `tools/soc_flash_app_safe.ps1`, `docs/review/flash_iap_address_gate_plan_2026-05-31.md`
最后更新时间：2026-05-31
未确认事项：本文只用于 S1 用户决策确认；未修改 `tools/project_check.py`、scatter、源码、Keil 工程或烧录脚本，未生成 `FD_Release.map`，未接 ST-Link/COM4/CAN/实物板。

## 1. 本确认包目标

S1 的目标不是立即改 scatter，也不是立即改源码，而是先确认 App/IAP/Flash 存储/mailbox 的发布安全边界。确认完成后，下一步只允许先改 `tools/project_check.py` 增加只读门禁；scatter 和源码必须另起阶段。

## 2. 当前源码事实摘要

| 事实 ID | 当前源码事实 | 证据 | 判断 |
|---|---|---|---|
| S1-FACT-001 | IAP/Bootloader 固定起始地址为 `0x08000000`，App 固定起始地址为 `0x08004800` | `Flash.h:4-5`, `tools/soc_flash_app_safe.ps1:17-20` | MUST_KEEP |
| S1-FACT-002 | 内部 Flash 持久化区从 `0x0801C000` 开始 | `Flash.h:7-29` | MUST_KEEP，但需要容量和链接边界门禁 |
| S1-FACT-003 | 当前 scatter 允许 `LR_IROM1/ER_IROM1 0x08004800 0x00020000`，理论结束到 `0x08024800` | `CommomSH367309_16series_103RCT6_C.sct:5-6` | CHANGE_NEEDED，至少需要门禁 |
| S1-FACT-004 | App->IAP SRAM mailbox 地址为 `0x20004FE0` | `Flash.c:12` | MUST_KEEP，除非 IAP 双端协议确认可变 |
| S1-FACT-005 | 当前 scatter 的 `RW_IRAM1 0x20000000 0x00005000` 覆盖到 `0x20005000`，包含 mailbox 尾部区间 | `CommomSH367309_16series_103RCT6_C.sct:12-14` | CHANGE_NEEDED，至少需要门禁 |
| S1-FACT-006 | `tools/project_check.py` 当前缺 `FD_Release.map` 时只 warning，并只检查 map 的 LR/ER base 和 ROM/RAM size | `tools/project_check.py:691-727` | 不足，不能证明发布安全 |

## 3. 需要用户确认的四个决定

| Decision ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| S1-DEC-001 | App 链接结束地址必须不覆盖 `0x0801C000+` 持久化区 | `Flash.h:7-29`; scatter `LR_IROM1 0x08004800 0x00020000` | 当前 scatter 理论允许 App 覆盖存储页；缺 `FD_Release.map`，无法证明当前产物没有覆盖 | P0：App 体积增长后覆盖 AFE/RW/log/SOC/aging 存储区 | CHANGE_NEEDED | 是否确认后 16KB 存储区固定保留，App load/exec 结束地址必须 `<= 0x0801C000`？ | 确认固定 `<= 0x0801C000`，先做 `project_check.py` 门禁，再另阶段改 scatter | |
| S1-DEC-002 | App->IAP mailbox `0x20004FE0` 必须被 App 保留 | `Flash.c:12`; scatter `RW_IRAM1 0x20000000 0x00005000` | mailbox 落在普通 RAM 尾部，当前 scatter 未显式 reserve | P0：IAP 请求可能被 RW/ZI 或运行期栈/全局变量覆盖，升级进入 IAP 不稳定 | CHANGE_NEEDED | IAP 固件是否固定读取 `0x20004FE0`？是否必须保留该地址并在 App scatter/map 中避让？ | 确认地址固定，后续先加 map 检查，再单独 reserve RAM 尾部 | |
| S1-DEC-003 | 发布前必须存在可解析且新鲜的 `FD_Release.map` | `tools/project_check.py:691-744` | 当前 map 缺失只 warning，仍可通过 quick check | P0/P1：无法证明 LR/ER base、App end、RW/ZI 是否安全 | CHANGE_NEEDED | 缺 `FD_Release.map` 时，是否允许发布？开发 quick 和发布 strict 是否区分？ | 开发 quick 只 warning；发布/strict 模式 fail | |
| S1-DEC-004 | 使用 `0x0801C000+` 后 64K Flash 存储前必须证明量产 MCU 容量 | `Flash.h:7-29`; `Flash.c` 读取 Flash size register | 文档尚未固定 BOM/芯片容量结论；本机未接板读取 | P0：若真实芯片不足 128KB，保存路径越界 | UNKNOWN | 量产 MCU 是否保证至少覆盖到 `0x08020000`？是否存在 C8/CB/大容量混批？ | 发布前要求 BOM + ST-Link/板端读数双证据；证据不足不发布 | |

## 4. 用户可直接填写的决策模板

| Decision ID | 我的决定 | 补充条件 |
|---|---|---|
| S1-DEC-001 |  |  |
| S1-DEC-002 |  |  |
| S1-DEC-003 |  |  |
| S1-DEC-004 |  |  |

建议默认填写：

- `S1-DEC-001`: 确认 App 结束地址必须 `<= 0x0801C000`。
- `S1-DEC-002`: 确认 mailbox 地址固定为 `0x20004FE0`，App 必须避让。
- `S1-DEC-003`: quick warning，发布/strict fail。
- `S1-DEC-004`: 必须有 BOM + 板端 Flash size 双证据；证据不足不发布。

## 5. 确认后的执行计划

| 阶段 | 文件范围 | 动作 | 禁止改动 | 风险 | 前置条件 | 验证 | 回滚 |
|---|---|---|---|---|---|---|---|
| S1-D1 | `tools/project_check.py`, 相关测试样例 | 增加 map 结束地址、mailbox、strict map 缺失检查；不动 scatter | 禁止改 Flash 地址、mailbox 地址、烧录脚本、源码 | 中 | 四个 S1 decision 已确认 | `python3 tools/project_check.py -q`; 构造缺 map/模拟 map 用例；`git diff --check` | revert 脚本 patch |
| S1-D2 | `103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.sct` | 确认脚本门禁后，单独收紧 App Flash 长度并 reserve SRAM mailbox | 禁止改 App 起始地址 `0x08004800`，禁止改 IAP 协议 | 高 | S1-D1 已通过，且用户批准改 scatter | Windows/Keil `FD_Release` 编译；解析 `FD_Release.map` | revert scatter patch |
| S1-D3 | Keil 构建产物和发布记录 | 生成并保存 `FD_Release.map/bin/hex` 证据 | 禁止绕过 `soc_flash_app_safe.ps1` | 中 | Windows/Keil 可用 | map/bin/vector/ROM/RAM 检查 | 不发布该产物 |
| S1-D4 | ST-Link/COM4/CAN 实机 | 只读验证 Flash size、`0xD000`、`0xD300`、IAP 入口 smoke | 禁止裸写 `0x08000000` | 高 | 用户确认接板和烧录范围 | ST-Link/串口/CAN 记录 | 重新烧录上一版 App |

## 6. 当前验证边界

已完成：

- 源码和脚本静态核对。
- S1 风险和用户确认项文档化。

未完成：

- 未修改 `tools/project_check.py`。
- 未修改 scatter。
- 未执行 Keil/ARMCC Release 编译。
- 未生成当前 `FD_Release.map`。
- 未读取板端 Flash size。
- 未烧录、未读 COM4、未抓 CAN、未验证 IAP 跳转。

因此，当前仍不能宣称 S1 完成，只能把本文作为进入 S1-D1 的确认输入。
