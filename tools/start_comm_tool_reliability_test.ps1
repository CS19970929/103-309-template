param(
    [string]$Port = "COM4",
    [int]$Baud = 115200,
    [string]$Bin = "103 + 309\Project\Users\Objects\FD_Release.bin",
    [int]$UpgradeLoops = 0,
    [switch]$DownloadCache,
    [switch]$ConfirmDownload,
    [switch]$ConfirmUpgrade
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$script = Join-Path $repoRoot "tools\comm_tool_reliability_test.py"

$argsList = @(
    "-3.9",
    $script,
    "--port", $Port,
    "--baud", "$Baud"
)

if ($Bin) {
    $binPath = (Resolve-Path (Join-Path $repoRoot $Bin)).Path
    $argsList += @("--bin", $binPath)
}
if ($DownloadCache) {
    $argsList += "--download-cache"
}
if ($ConfirmDownload) {
    $argsList += "--confirm-download"
}
if ($UpgradeLoops -gt 0) {
    $argsList += @("--upgrade-loops", "$UpgradeLoops")
}
if ($ConfirmUpgrade) {
    $argsList += "--confirm-upgrade"
}

& py @argsList
