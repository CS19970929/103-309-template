# MCU SOC测试安全烧录说明

## 地址边界

当前工程不是从 `0x08000000` 直接运行 App，而是 IAP + App 结构：

- IAP/Bootloader：`0x08000000`
- App：`0x08004800`
- Scatter 文件：`103 + 309/Project/Users/Objects/FD_Release.sct`

因此：

- 不要把 `FD_Release.bin` 或 `FD_Debug.bin` 写到 `0x08000000`。
- 裸 `.bin` 没有地址信息，必须显式写到 `0x08004800`。
- `.hex` 带地址信息，可以按 HEX 内部地址烧录，但仍要确认 HEX 起始地址是 `0x08004800`。

## 正常程序恢复流程

1. 先烧 IAP 到 `0x08000000`。
2. 再烧 App 到 `0x08004800`。
3. 上电或复位后，通过 `COM4 / 19200 / slave=1` 读取 `0xD000`，确认正常通信。

## SOC测试固件配置

测试固件需要临时打开：

```c
#define PROJECT_CFG_BUILD_PROFILE 2
#define PROJECT_CFG_SOC_TEST_MODE_ENABLE 1
#define PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX 300
```

量产配置必须保持：

```c
#define PROJECT_CFG_BUILD_PROFILE 0
/* PROJECT_CFG_SOC_TEST_MODE_ENABLE 默认 0 或不定义 */
```

`Project_BuildGuard.h` 已经限制 Release 构建不能打开测试模式。

## 安全烧录 App

先编译 App：

```powershell
& "C:\Keil_v5\UV4\UV4.exe" -r "CommomSH367309_16series_103RCT6_C.uvprojx" -t "FD_Release" -o "keil_rebuild_soc_test_app_08004800.log"
```

再只写 App 区：

```powershell
.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash
```

该脚本固定校验 App 地址为 `0x08004800`，会拒绝覆盖 `0x08000000`。

等价的底层命令是：

```powershell
STM32_Programmer_CLI.exe -c port=SWD -w "...\FD_Release.bin" 0x08004800 -v -rst
```

## 上位机确认

烧入测试 App 后：

1. 打开 `py tools\soc_test_ui.py`。
2. 在线板端监控页读取 `0xD000`。
3. MCU加速测试页读取 `0xD300`。
4. `supported=1` 后再运行真实骑行或快变电流测试。

如果 `0xD300` 显示 `supported=0`，说明当前仍是正常程序或测试模式未开启。

## 本次现场结论

- 正常程序恢复后可在线读取：`SOC=60%`、`SOH=100%`、`Vmin≈3677mV`、`Vmax≈3678mV`。
- 正常程序下 `0xD300` 返回 `supported=0`，符合量产隔离。
- 尝试用 ST-Link 写 App 区时，工具返回 `DEV_TARGET_CMD_ERR`，说明当前调试连接需要硬件侧复位/低功耗处理后再烧测试 App。
