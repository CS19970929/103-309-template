# 仓库协作规则

- 全部使用中文回答。
- 进行较大的代码变更时，需要同步生成说明文档，并自动创建描述清楚的 git 提交。
- 不要依赖单次对话记忆。涉及烧录地址、测试模式、量产隔离、上位机启动方式的规则必须写入仓库脚本或文档。

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
