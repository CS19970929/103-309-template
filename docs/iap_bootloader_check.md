# IAP / Bootloader 检查

记录日期：2026-05-31

## 结论

项目存在 IAP/Bootloader，App 不从 `0x08000000` 启动，必须从 `0x08004800` 启动。

## 证据

| 检查项 | 证据 |
|---|---|
| IAP 起始 | `Flash.h`：`FLASH_ADDR_IAP_START 0x08000000` |
| App 起始 | `Flash.h`：`FLASH_ADDR_APP_START 0x08004800` |
| Keil scatter | `LR_IROM1 0x08004800` |
| VTOR 偏移 | `system_stm32f10x.c`：`VECT_TAB_OFFSET 0x4800` |
| `_IAP` 来源 | `Project_Config.h` 中 `PROJECT_CFG_IAP_ENABLE 1`，`conf.h` 派生 `_IAP` |
| App 跳 IAP | `Flash.c` 使用 `FLASH_ADDR_IAP_START` 读取 MSP/ResetHandler 并 `__set_MSP` 后跳转 |
| IAP 标志 | `FLASH_ADDR_UPDATE_FLAG 0x0801F800`，`FLASH_TO_IAP_VALUE 0x00AB` |

## GCC 迁移要求

- Linker script 的 App 起点必须是 `0x08004800`。
- `.isr_vector` 必须位于 `0x08004800`。
- `system_stm32f10x.c` 必须继续设置 `SCB->VTOR = FLASH_BASE | 0x4800`。
- `.bin` 是裸二进制，烧录脚本必须显式写入 `0x08004800`。
- `.hex` 应由 ELF 地址生成，保留 `0x08004800` 装载地址。
- 默认烧录不得写 `0x08000000`，不得默认全片擦除。

## 已验证项

- Debug/Release GCC ELF 均已从 `0x08004800` 生成。
- `scripts/flash.py` 默认 raw bin 地址为 `0x08004800`。
- `scripts/flash.py --address 0x08000000` 会拒绝执行并提示覆盖 IAP 风险。
- dry-run 不要求本机已安装烧录工具，便于先审查命令和地址。
- ST-LINK 实烧 Release 后，`0x08000000` 向量保持不变，`0x08004800` 向量更新，`0x0801C000` 参数区头部保持不变。
- GDB 复位后可以从 IAP 继续运行并命中 App `main`。

## 风险

- 高风险：如果 CMake 宏/include 导致 `_IAP` 未定义，VTOR 会被设置到 `0x08000000`，App 中断会异常。
- 高风险：如果将 `.bin` 裸写到 `0x08000000` 会覆盖 IAP。
- 中风险：Boot 跳 App 所需 MSP/ResetHandler 必须与 GCC startup 向量表前两项一致。
