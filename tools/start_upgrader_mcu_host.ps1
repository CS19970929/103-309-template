param(
    [ValidateSet("detect", "info", "snapshot", "read-object", "write-object", "read-param", "write-param", "upgrade-dry-run", "upgrade")]
    [string]$Mode = "detect",
    [string]$Port = "",
    [int]$Baud = 115200,
    [string]$Bin = "",
    [string]$Index = "",
    [string]$Chd = "",
    [string]$Data = "",
    [string]$ParamId = "",
    [int]$RawValue = 0,
    [switch]$Confirm,
    [switch]$EnterIap,
    [string]$PythonVersion = "3.9"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Py = (Get-Command py.exe -ErrorAction Stop).Source

Push-Location $RepoRoot
try {
    $ArgsList = @("-$PythonVersion", "tools\upgrader_mcu_host.py", $Mode)

    if ($Mode -ne "detect" -and $Mode -ne "upgrade-dry-run") {
        if ([string]::IsNullOrWhiteSpace($Port)) {
            throw "Mode=$Mode 需要 -Port，例如 COM8"
        }
        $ArgsList += @("--port", $Port, "--baud", [string]$Baud)
    }

    if ($Mode -eq "read-object" -or $Mode -eq "write-object") {
        if ([string]::IsNullOrWhiteSpace($Index) -or [string]::IsNullOrWhiteSpace($Chd)) {
            throw "Mode=$Mode 需要 -Index 和 -Chd"
        }
        $ArgsList += @("--index", $Index, "--chd", $Chd)
    }

    if ($Mode -eq "write-object") {
        if ([string]::IsNullOrWhiteSpace($Data)) {
            throw "Mode=write-object 需要 -Data"
        }
        $ArgsList += @("--data", $Data)
    }

    if ($Mode -eq "read-param" -or $Mode -eq "write-param") {
        if ([string]::IsNullOrWhiteSpace($ParamId)) {
            throw "Mode=$Mode 需要 -ParamId"
        }
        $ArgsList += @("--param-id", $ParamId)
    }

    if ($Mode -eq "write-param") {
        $ArgsList += @("--raw-value", [string]$RawValue)
        if ($Confirm) {
            $ArgsList += "--confirm"
        }
    }

    if ($Mode -eq "upgrade-dry-run" -or $Mode -eq "upgrade") {
        if ([string]::IsNullOrWhiteSpace($Bin)) {
            throw "Mode=$Mode 需要 -Bin"
        }
        $ArgsList += @("--bin", $Bin)
    }

    if ($Mode -eq "upgrade" -and $EnterIap) {
        $ArgsList += "--enter-iap"
    }

    & $Py @ArgsList
    exit $LASTEXITCODE
}
catch {
    Write-Error @"
升级器 MCU 串口工具启动失败。

依赖安装：
  py -$PythonVersion -m pip install pyserial

错误信息：
$($_.Exception.Message)
"@
    exit 1
}
finally {
    Pop-Location
}
