param(
    [ValidateSet("detect", "listen", "app-read-status", "app-enter-iap", "app-write-soc", "app-aging-start", "app-aging-stop", "app-aging-reset-time", "app-aging-set-hours", "upgrade-dry-run", "upgrade")]
    [string]$Mode = "detect",
    [string]$Interface = "pcan",
    [string]$Channel = "PCAN_USBBUS1",
    [int]$Bitrate = 250000,
    [double]$Duration = 10.0,
    [string]$Bin = "",
    [int]$NodeId = 1,
    [int]$CanAddress = 0,
    [int]$Soc = -1,
    [int]$AgingHours = -1,
    [string]$ConfirmAppAddress = "",
    [string]$PythonVersion = "3.9",
    [switch]$WaitAck,
    [switch]$ConfirmEnterIap,
    [switch]$ConfirmWriteSoc,
    [switch]$ConfirmAgingStart,
    [switch]$ConfirmAgingStop,
    [switch]$ConfirmAgingResetTime,
    [switch]$ConfirmAgingSetHours
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

    $AppModes = @("app-read-status", "app-enter-iap", "app-write-soc", "app-aging-start", "app-aging-stop", "app-aging-reset-time", "app-aging-set-hours")

    if ($Mode -eq "listen" -or $Mode -eq "upgrade" -or ($AppModes -contains $Mode)) {
        $CommonArgs += @(
            "--interface", $Interface,
            "--channel", $Channel,
            "--bitrate", [string]$Bitrate
        )
    }

    if ($Mode -eq "listen") {
        $CommonArgs += @("--duration", [string]$Duration)
    }

    if ($AppModes -contains $Mode) {
        $CommonArgs += @("--can-address", [string]$CanAddress)
    }

    if ($Mode -eq "app-enter-iap") {
        if ($ConfirmEnterIap) {
            $CommonArgs += "--confirm-enter-iap"
        }
    }

    if ($Mode -eq "app-write-soc") {
        if ($Soc -lt 0 -or $Soc -gt 100) {
            throw "Mode=app-write-soc 需要 -Soc 0..100"
        }
        $CommonArgs += @("--soc", [string]$Soc)
        if ($ConfirmWriteSoc) {
            $CommonArgs += "--confirm-write-soc"
        }
    }

    if ($Mode -eq "app-aging-start") {
        if ($ConfirmAgingStart) {
            $CommonArgs += "--confirm-aging-start"
        }
    }

    if ($Mode -eq "app-aging-stop") {
        if ($ConfirmAgingStop) {
            $CommonArgs += "--confirm-aging-stop"
        }
    }

    if ($Mode -eq "app-aging-reset-time") {
        if ($ConfirmAgingResetTime) {
            $CommonArgs += "--confirm-aging-reset-time"
        }
    }

    if ($Mode -eq "app-aging-set-hours") {
        if ($AgingHours -lt 1 -or $AgingHours -gt 168) {
            throw "Mode=app-aging-set-hours 需要 -AgingHours 1..168"
        }
        $CommonArgs += @("--aging-hours", [string]$AgingHours)
        if ($ConfirmAgingSetHours) {
            $CommonArgs += "--confirm-aging-set-hours"
        }
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
