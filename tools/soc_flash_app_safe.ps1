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
if ($appAddress -ne 0x08004800) {
    throw "Refuse to flash SOC app at $Address. This project app starts at 0x08004800; 0x08000000 is the IAP area."
}

$args = @(
    "-c", "port=SWD",
    "-w", $resolvedBin.Path,
    $Address,
    "-v",
    "-rst"
)

Write-Host "SOC app flash target:"
Write-Host "  bin:     $($resolvedBin.Path)"
Write-Host "  address: $Address"
Write-Host "  note:    IAP stays at 0x08000000; only app area is programmed."

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
