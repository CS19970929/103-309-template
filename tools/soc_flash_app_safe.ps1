param(
    [string]$Bin = "103 + 309\Project\Users\Objects\FD_Release.bin",
    [string]$Address = "0x08004800",
    [string]$Programmer = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
    [switch]$Flash
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

$resolvedBin = Resolve-Path -LiteralPath $Bin
if (-not (Test-Path -LiteralPath $Programmer)) {
    throw "STM32CubeProgrammer CLI not found: $Programmer"
}

$appAddress = [Convert]::ToUInt32($Address.Replace("0x", ""), 16)
$appStorageBoundary = [uint32]0x0800E000
$appMaxSize = [uint32]0x00009800
if ($appAddress -ne 0x08004800) {
    throw "Refuse to flash SOC app at $Address. This project app starts at 0x08004800; 0x08000000 is the IAP area."
}

$binSize = [uint32](Get-Item -LiteralPath $resolvedBin.Path).Length
$appEnd = [uint64]$appAddress + [uint64]$binSize
if (($binSize -gt $appMaxSize) -or ($appEnd -gt [uint64]$appStorageBoundary)) {
    $message = ("Refuse to flash oversized app: size={0} bytes, max={1} bytes, end=0x{2:X8}. " +
                "Persistent storage starts at 0x0800E000 and must never be overwritten.") -f $binSize, $appMaxSize, $appEnd
    throw $message
}

$args = @(
    "-c", "port=SWD",
    "-w", $resolvedBin.Path,
    $Address,
    "-v",
    "-rst"
)

Write-Host "SOC app flash target:"
Write-Host "  bin:       $($resolvedBin.Path)"
Write-Host "  size:      $binSize bytes / $appMaxSize bytes max"
Write-Host "  address:   $Address"
Write-Host ("  app end:   0x{0:X8}" -f $appEnd)
Write-Host "  storage:   0x0800E000..0x0800FFFF (official front 64KB only)"
Write-Host "  note:      IAP stays at 0x08000000; storage pages are excluded."

if (-not $Flash) {
    Write-Host ""
    Write-Host "Dry run only. Re-run with -Flash to execute:"
    Write-Host "`"$Programmer`" $($args -join ' ')"
    exit 0
}

& $Programmer @args
if ($LASTEXITCODE -ne 0) {
    throw "STM32CubeProgrammer failed with exit code $LASTEXITCODE"
}
