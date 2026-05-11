param(
    [ValidateSet("detect", "listen", "upgrade-dry-run", "upgrade")]
    [string]$Mode = "detect",
    [string]$Interface = "pcan",
    [string]$Channel = "PCAN_USBBUS1",
    [int]$Bitrate = 500000,
    [double]$Duration = 10.0,
    [string]$Bin = "",
    [int]$NodeId = 1,
    [string]$ConfirmAppAddress = "",
    [string]$PythonVersion = "3.9",
    [switch]$WaitAck
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Py = (Get-Command py.exe -ErrorAction Stop).Source

Push-Location $RepoRoot
try {
    $CommonArgs = @(
        "-$PythonVersion",
        "tools\can_bms_host.py",
        $Mode
    )

    if ($Mode -eq "listen" -or $Mode -eq "upgrade") {
        $CommonArgs += @(
            "--interface", $Interface,
            "--channel", $Channel,
            "--bitrate", [string]$Bitrate
        )
    }

    if ($Mode -eq "listen") {
        $CommonArgs += @("--duration", [string]$Duration)
    }

    if ($Mode -eq "upgrade-dry-run" -or $Mode -eq "upgrade") {
        if ([string]::IsNullOrWhiteSpace($Bin)) {
            throw "Mode=$Mode 需要提供 -Bin 参数"
        }
        $CommonArgs += @(
            "--bin", $Bin,
            "--node-id", [string]$NodeId
        )
    }

    if ($Mode -eq "upgrade") {
        $CommonArgs += @("--confirm-app-address", $ConfirmAppAddress)
        if ($WaitAck) {
            $CommonArgs += "--wait-ack"
        }
    }

    & $Py @CommonArgs
    exit $LASTEXITCODE
}
catch {
    Write-Error @"
CAN BMS 上位机启动失败。

依赖安装：
  py -$PythonVersion -m pip install python-can

PCAN/Kvaser/CANable 还需要安装对应驱动，并确认 -Interface / -Channel 参数。

错误信息：
$($_.Exception.Message)
"@
    exit 1
}
finally {
    Pop-Location
}
