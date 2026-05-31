# Heat_Cool 与 IODrivers 删除说明

## 结论

`Heat_Cool.c/.h` 和 `IODrivers.c/.h` 已从当前工程删除。当前量产配置不启用 Heat_Cool，IODrivers 也没有被业务代码调用；删除后可减少无效源码、Keil 工程项和旧协议入口，降低后续维护误启用风险。

## 删除范围

- 删除源码文件：
  - `103 + 309/Project/Source/Heat_Cool.c`
  - `103 + 309/Project/Source/Heat_Cool.h`
  - `103 + 309/Project/Source/IODrivers.c`
  - `103 + 309/Project/Source/IODrivers.h`
- 删除 Keil `FD_Debug` / `FD_Release` 目标里的对应工程项。
- 删除 `main.h` 对 `Heat_Cool.h` 的包含。
- 删除 `InitHeat_Cool()` 和 `App_Heat_Cool_Ctrl()` 的初始化/主循环/STOP 唤醒恢复入口。
- 删除 `PROJECT_CFG_HEAT_ENABLE` 到 `__FUNC__HEAT__` 的编译期开关链路。
- 删除上位机 `0x10` 写 Heat/Cool 参数和 `0x06/0x1004` 恢复 Heat/Cool 默认值的处理函数。

## 协议变化

- `0x2300 ~ 0x231F` 仍为 `OtherElement` 可读写参数区。
- 原 `0x2320 ~ 0x2337` Heat/Cool 参数区不再开放；读写会按地址/权限校验走负响应。
- 原 `0x1004` Heat/Cool 恢复默认命令保留为空洞，后续命令地址不迁移：
  - `0x1005` 仍是 `RS485_CMD_ADDR_SET_ONCE_SOC`
  - `0x1006` 仍是 `RS485_CMD_ADDR_RESET_AFE_PARAMETERS`
  - `0x1007` 仍是 `RS485_CMD_ADDR_RESET_EVENT_RECORD`
- `0xD000` 只读运行数据里原 Heat/Cool 故障字固定输出 `0`，不再依赖已删除的 `Heat_Cool_FaultFlag`。

## Flash 参数兼容

内部 Flash 的 RW 参数结构仍保留原 24 个字的尾部占位：

```c
typedef struct
{
    UINT16 protect[65];
    UINT16 other[32];
    UINT16 reserved[24];
} STORAGE_FLASH_RW_PARAM_DATA;
```

这样做的目的：

- 保持 `STORAGE_FLASH_RW_PARAM_DATA` 总长度不变，避免升级后旧设备已有的保护参数、OtherElement 参数因结构长度变化而整体失效。
- 加载旧 Flash 数据时忽略保留区，只校验和应用 `protect[]`、`other[]`。
- 保存新参数时保留区写入 `0xFFFF`，不再承载 Heat/Cool 业务含义。

## 自动检查

`tools/project_check.py` 已加入删除守卫：

- 检查四个源文件不能重新出现在源码树。
- 检查 Keil 工程不能重新引用 `Heat_Cool.c` 或 `IODrivers.c`。
- 检查 C/H 源码中不能重新出现 `Heat_Cool`、`HeatCool`、`IODrivers`、`__FUNC__HEAT__`、`PROJECT_CFG_HEAT_ENABLE` 等旧符号。

## 验证项

- 编译 `FD_Release` 通过。
- `FD_Release.map` 中不再出现 `Heat_Cool`、`IODrivers`、`InitHeat_Cool`、`App_Heat_Cool_Ctrl`。
- `tools/project_check.py -q` 通过。
- `git diff --check` 通过。
