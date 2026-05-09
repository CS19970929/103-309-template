# SOC 测试上位机 EXE 打包说明

## 产物路径

打包后的上位机路径：

```text
dist\BMS_SOC_Test_UI.exe
```

可直接双击运行，也可以命令行运行：

```powershell
.\dist\BMS_SOC_Test_UI.exe
```

## 一键打包

在仓库根目录执行：

```powershell
.\tools\build_soc_test_ui_exe.ps1 -Clean
```

脚本固定使用 `py -3.9`，并检查：

- `pyserial`
- `tkinter`
- `PyInstaller`

如果缺少 PyInstaller，脚本会自动安装。

## EXE 运行目录

EXE 运行后，报告和数据文件会写到 EXE 所在目录：

```text
dist\SOC_RIDE_SIM_REPORT.md
dist\SOC_RIDE_SIM_SAMPLES.csv
dist\SOC_ONLINE_MONITOR.csv
```

这样便于把整个 `dist` 目录拷贝到其他电脑使用。

## 注意事项

- EXE 已内置 Python 运行环境和 `pyserial`，使用时不需要再安装 Python。
- 在线监控仍然需要电脑能识别串口设备，例如 `COM4`。
- 量产固件下 MCU 加速测试状态 `supported=0` 是正常现象，表示测试入口关闭，不影响出货程序。
