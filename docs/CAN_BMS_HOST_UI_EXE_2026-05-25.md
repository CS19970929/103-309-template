# CAN 上位机用户版 EXE

## 固定产物

CAN 上位机给用户使用的文件固定为：

```text
dist\BMS_CAN_Host_UI.exe
```

这个 exe 是图形界面，首页直接提供常用功能：

- 读取 `SOC/SOH`
- 写 SOC
- 开启老化模式
- 关闭老化模式
- 重置老化时间
- 监听 CAN 广播，并显示老化剩余时间

## 编译规则

以后只要修改 CAN 用户上位机相关代码，必须同步编译最新 exe，不能只提交 `.py` 或 `.ps1`：

```powershell
.\tools\build_can_bms_host_ui_exe.ps1 -Clean
```

需要编译完直接打开时使用：

```powershell
.\tools\build_can_bms_host_ui_exe.ps1 -Clean -Run
```

打包脚本会检查并安装当前 Python 环境缺少的 `python-can` 和 `pyinstaller`，并使用 PyInstaller 生成单文件窗口程序。

## 使用说明

默认 CAN 参数：

- interface: `pcan`
- channel: `PCAN_USBBUS1`
- bitrate: `250000`
- CAN 地址: `0`

如果用户使用 Kvaser、CANable/slcan 或 virtual CAN，需要在界面顶部修改接口和通道。

写 SOC 是单独按钮，不要求用户手动写寄存器。底层固定写 `0x1005 RS485_CMD_ADDR_SET_ONCE_SOC`。

老化模式三个动作必须作为单独按钮保留，不能合并成一个通用 action 入口。
