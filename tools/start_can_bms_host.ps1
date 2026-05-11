param(
    [ValidateSet("detect", "listen", "app-read-status", "app-enter-iap", "upgrade-dry-run", "upgrade")]
    [string]$Mode = "detect",
    [string]$Interface = "pcan",
    [string]$Channel = "PCAN_USBBUS1",
    [int]$Bitrate = 250000,
    [double]$Duration = 10.0,
    [string]$Bin = "",
    [int]$HostNode = 0x10,
    [int]$DeviceNode = 0x14,
    [Nullable[int]]$NodeId = $null,
    [int]$CanAddress = 0,
    [ValidateSet("0", "1")]
    [string]$LongIndexBase = "0",
    [string]$ConfirmAppAddress = "",
    [string]$PythonVersion = "3.9",
    [switch]$WaitAck,
    [switch]$NoWaitAck,
    [switch]$ConfirmEnterIap
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

    if ($Mode -eq "listen" -or $Mode -eq "upgrade" -or $Mode -eq "app-read-status" -or $Mode -eq "app-enter-iap") {
        $CommonArgs += @(
            "--interface", $Interface,
            "--channel", $Channel,
            "--bitrate", [string]$Bitrate
        )
    }

    if ($Mode -eq "listen") {
        $CommonArgs += @("--duration", [string]$Duration)
    }

    if ($Mode -eq "app-read-status" -or $Mode -eq "app-enter-iap") {
        $CommonArgs += @("--can-address", [string]$CanAddress)
    }

    if ($Mode -eq "app-enter-iap") {
        if ($ConfirmEnterIap) {
            $CommonArgs += "--confirm-enter-iap"
        }
    }

    if ($Mode -eq "upgrade-dry-run" -or $Mode -eq "upgrade") {
        if ([string]::IsNullOrWhiteSpace($Bin)) {
            throw "Mode=$Mode 需要提供 -Bin 参数"
        }
        $CommonArgs += @(
            "--bin", $Bin,
            "--host-node", ("0x{0:X}" -f $HostNode),
            "--device-node", ("0x{0:X}" -f $DeviceNode),
            "--long-index-base", $LongIndexBase
        )
        if ($null -ne $NodeId) {
            $CommonArgs += @("--node-id", ("0x{0:X}" -f $NodeId.Value))
        }
    }

    if ($Mode -eq "upgrade") {
        $CommonArgs += @("--confirm-app-address", $ConfirmAppAddress)
        if ($NoWaitAck) {
            $CommonArgs += "--no-wait-ack"
        }
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
