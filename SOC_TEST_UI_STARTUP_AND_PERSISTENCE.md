# SOC 测试上位机启动与跨对话规则

## 为什么换对话后还要写文件

Codex 换一个新对话后，默认不会自动知道上一轮聊天里的全部细节。能稳定传递给新对话的是仓库里的文件、脚本、提交记录和项目说明。因此烧录地址、量产隔离、上位机启动方式不能只靠聊天记忆，必须固化到 `AGENTS.md`、脚本和测试文档里。

本次已经把关键约束写入 `AGENTS.md`：

- App 只能烧到 `0x08004800`，IAP/Bootloader 是 `0x08000000`。
- 禁止把裸 `FD_Debug.bin` 或 `FD_Release.bin` 写到 `0x08000000`。
- 正常量产程序必须关闭 SOC MCU 注入式测试入口。
- 上位机必须用固定脚本启动，避免用错 Python 环境。

## 上位机固定启动

不要直接双击 `tools\soc_test_ui.py`。双击可能调用系统里另一个 Python，导致串口打开时报：

```text
在线监控失败: No module named 'serial'
```

请在仓库根目录执行：

```powershell
.\tools\start_soc_test_ui.ps1
```

自动演示完整流程：

```powershell
.\tools\start_soc_test_ui.ps1 -Demo -Port COM4 -Baud 19200 -Slave 1 -Samples 10 -Interval 0.5
```

脚本会先检查 `pyserial` 和 `tkinter`，再启动 UI。若依赖缺失，执行：

```powershell
py -3.9 -m pip install pyserial
```

原因：桌面可见环境里 `py` 默认可能指向 Python 3.12，而当前已验证可用的上位机环境是 `py -3.9`。

## 自动演示会做什么

自动演示模式会在 UI 内依次执行：

1. 真实骑行场景仿真，生成 `SOC_RIDE_SIM_REPORT.md` 和 `SOC_RIDE_SIM_SAMPLES.csv`。
2. 切换到在线监控页，读取板端 SOC 参数。
3. 读取 MCU SOC 测试模式状态。
4. 采集在线监控数据，生成 `SOC_ONLINE_MONITOR.csv`。

## 量产程序下的预期结果

正常量产程序读取 `0xD300` 时应显示 `supported=0`，表示 MCU 注入式 SOC 加速测试入口关闭。这是正确的隔离结果，说明测试模式不会影响出货量产程序。

如果要让 MCU 参与“真实骑行电压/电流注入”测试，必须使用测试固件，并确认：

- `PROJECT_CFG_BUILD_PROFILE=2`
- `PROJECT_CFG_SOC_TEST_MODE_ENABLE=1`
- App 仍烧录到 `0x08004800`
- 测试结束后恢复量产固件并再次确认 `supported=0`
