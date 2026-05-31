# 工具链安装说明

记录日期：2026-05-31

## 目标工具

必须安装：

- Python 3.9 或更新版本。
- CMake。
- Ninja。
- Arm GNU Toolchain，必须包含 `arm-none-eabi-gcc`、`arm-none-eabi-objcopy`、`arm-none-eabi-size`、`arm-none-eabi-gdb`。

可选安装：

- SEGGER J-Link Software，用于 J-Link 烧录和调试。
- STM32CubeProgrammer，用于 ST-LINK 烧录。
- OpenOCD，用于 ST-LINK/OpenOCD 调试路径。

官方入口：

- [Arm GNU Toolchain Downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
- [CMake Download](https://cmake.org/download/)
- [Ninja Releases](https://github.com/ninja-build/ninja/releases)
- [SEGGER J-Link Software](https://www.segger.com/downloads/jlink/)
- [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html)
- [OpenOCD](https://openocd.org/)

## Windows

推荐安装方式：

1. 安装 Python 3.9+，并确认 `python --version` 可用。若使用 Windows Python Launcher，也应确认 `py -3.9 --version` 可用。
2. 安装 CMake，并勾选加入 PATH。
3. 安装 Ninja，并将 `ninja.exe` 所在目录加入 PATH。
4. 安装 Arm GNU Toolchain。
5. 设置环境变量 `ARM_GNU_TOOLCHAIN_PATH` 指向工具链根目录，例如：

```powershell
[Environment]::SetEnvironmentVariable("ARM_GNU_TOOLCHAIN_PATH", "C:\Program Files\Arm GNU Toolchain arm-none-eabi", "User")
```

实际路径必须以本机安装目录为准。设置后重新打开终端。

检查：

```powershell
python scripts\check_env.py
```

如果 `python` 指向 WindowsApps 占位程序，需要安装正式 Python 或在 VS Code 的 `.vscode/settings.json` 中把 `python.defaultInterpreterPath` 改成本机真实解释器路径。

## macOS

可使用 Homebrew 安装通用构建工具：

```bash
brew install cmake ninja
```

Arm GNU Toolchain 建议使用 Arm 官方 macOS 包，安装后设置：

```bash
export ARM_GNU_TOOLCHAIN_PATH="/path/to/arm-gnu-toolchain"
export PATH="$ARM_GNU_TOOLCHAIN_PATH/bin:$PATH"
```

如果希望永久生效，将上述内容写入 `~/.zshrc` 或当前 shell 的启动文件。

检查：

```bash
python3 scripts/check_env.py
```

## 工具链路径规则

- CMake 不写死工具链绝对路径。
- 优先使用 `ARM_GNU_TOOLCHAIN_PATH`。
- 如果未设置环境变量，则依赖 PATH 自动搜索。
- VS Code 调试默认调用 `arm-none-eabi-gdb`，如不在 PATH，需要在用户本地 settings 中指定完整路径。
