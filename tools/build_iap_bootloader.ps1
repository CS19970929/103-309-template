param(
    [string]$KeilRoot = "C:\Keil_v5",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

function Invoke-Tool {
    param(
        [string]$Exe,
        [string[]]$ToolArgs,
        [string]$FailMessage
    )

    & $Exe @ToolArgs
    if ($LASTEXITCODE -ne 0) {
        throw $FailMessage
    }
}

function Copy-RequiredFile {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (-not (Test-Path $Source)) {
        throw "missing source: $Source"
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$ProjectRoot = Join-Path $RepoRoot "103 + 309\Project"
$IapRoot = Join-Path $ProjectRoot "IAP"
$LibRoot = Join-Path $ProjectRoot "STM32F10x_StdPeriph_Lib_V3.5.0"
$BuildRoot = Join-Path $ProjectRoot "Users\Objects\IAP"
$ListingRoot = Join-Path $ProjectRoot "Users\Listings\IAP"
$StageRoot = Join-Path $env:TEMP "codex_iap_bootloader_f103c8"

$ArmBin = Join-Path $KeilRoot "ARM\ARMCC\bin"
$ArmCc = Join-Path $ArmBin "armcc.exe"
$ArmAsm = Join-Path $ArmBin "armasm.exe"
$ArmLink = Join-Path $ArmBin "armlink.exe"
$FromElf = Join-Path $ArmBin "fromelf.exe"

foreach ($tool in @($ArmCc, $ArmAsm, $ArmLink, $FromElf)) {
    if (-not (Test-Path $tool)) {
        throw "Keil ARMCC tool not found: $tool"
    }
}

if ($Clean -and (Test-Path $BuildRoot)) {
    $resolvedBuild = (Resolve-Path $BuildRoot).Path
    $resolvedProject = (Resolve-Path $ProjectRoot).Path
    if (-not $resolvedBuild.StartsWith($resolvedProject, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "refuse to clean unexpected directory: $resolvedBuild"
    }
    Remove-Item -LiteralPath $BuildRoot -Recurse -Force
}

if (Test-Path $StageRoot) {
    Remove-Item -LiteralPath $StageRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $BuildRoot, $ListingRoot | Out-Null
New-Item -ItemType Directory -Force -Path `
    (Join-Path $StageRoot "iap"), `
    (Join-Path $StageRoot "stm32\drivers"), `
    (Join-Path $StageRoot "stm32\inc"), `
    (Join-Path $StageRoot "stm32\src"), `
    (Join-Path $StageRoot "obj") | Out-Null

Copy-Item -Path (Join-Path $IapRoot "*") -Destination (Join-Path $StageRoot "iap") -Recurse -Force
Copy-Item -Path (Join-Path $LibRoot "inc\*.h") -Destination (Join-Path $StageRoot "stm32\inc") -Force

foreach ($file in @("core_cm3.h", "stm32f10x.h", "stm32f10x_conf.h", "system_stm32f10x.h", "startup_stm32f10x_hd.s")) {
    Copy-RequiredFile (Join-Path $LibRoot "drivers\$file") (Join-Path $StageRoot "stm32\drivers")
}

foreach ($file in @("misc.c", "stm32f10x_can.c", "stm32f10x_flash.c", "stm32f10x_gpio.c", "stm32f10x_rcc.c")) {
    Copy-RequiredFile (Join-Path $LibRoot "src\$file") (Join-Path $StageRoot "stm32\src")
}

Push-Location $StageRoot
try {
    $commonCompileArgs = @(
        "--cpu", "Cortex-M3",
        "--apcs=interwork",
        "--c99",
        "-O2",
        "--split_sections",
        "--library_type=microlib",
        "--diag_suppress=1",
        "-DSTM32F10X_MD",
        "-DUSE_STDPERIPH_DRIVER",
        "-Iiap",
        "-Istm32\drivers",
        "-Istm32\inc"
    )

    $sources = @(
        "iap\iap_main.c",
        "iap\iap_can_upgrade.c",
        "iap\iap_flash.c",
        "iap\iap_crc16.c",
        "iap\iap_system_stm32f10x.c",
        "iap\iap_stm32f10x_it.c",
        "stm32\src\misc.c",
        "stm32\src\stm32f10x_can.c",
        "stm32\src\stm32f10x_flash.c",
        "stm32\src\stm32f10x_gpio.c",
        "stm32\src\stm32f10x_rcc.c"
    )

    $objects = @()
    foreach ($source in $sources) {
        $name = [System.IO.Path]::GetFileNameWithoutExtension($source)
        $object = "obj\$name.o"
        $objects += $object
        Invoke-Tool $ArmCc ($commonCompileArgs + @("-c", $source, "-o", $object)) "compile failed: $source"
    }

    $startupObj = "obj\startup_stm32f10x_hd.o"
    Invoke-Tool $ArmAsm @("--cpu", "Cortex-M3", "--apcs=interwork", "--pd", "__MICROLIB SETA 1", "stm32\drivers\startup_stm32f10x_hd.s", "-o", $startupObj) "assemble failed: startup"
    $objects += $startupObj

    $axf = "obj\FD_IAP.axf"
    $bin = "obj\FD_IAP.bin"
    $map = "obj\FD_IAP.map"
    $linkArgs = @(
        "--cpu", "Cortex-M3",
        "--library_type=microlib",
        "--scatter", "iap\FD_IAP.sct",
        "--map",
        "--xref",
        "--callgraph",
        "--symbols",
        "--info", "sizes",
        "--list", $map
    ) + $objects + @("-o", $axf)

    Invoke-Tool $ArmLink $linkArgs "link failed: $axf"
    Invoke-Tool $FromElf @("--bin", "-o", $bin, $axf) "fromelf failed: $bin"

    Copy-Item -LiteralPath $axf -Destination (Join-Path $BuildRoot "FD_IAP.axf") -Force
    Copy-Item -LiteralPath $bin -Destination (Join-Path $BuildRoot "FD_IAP.bin") -Force
    Copy-Item -LiteralPath $map -Destination (Join-Path $ListingRoot "FD_IAP.map") -Force
}
finally {
    Pop-Location
}

$binPath = Join-Path $BuildRoot "FD_IAP.bin"
$binInfo = Get-Item $binPath
if ($binInfo.Length -gt 0x4800) {
    throw "IAP image too large: $($binInfo.Length) bytes, limit is 18432 bytes"
}

Write-Host "IAP build success"
Write-Host "  AXF: $(Join-Path $BuildRoot "FD_IAP.axf")"
Write-Host "  BIN: $binPath"
Write-Host "  MAP: $(Join-Path $ListingRoot "FD_IAP.map")"
Write-Host "  BIN Size: $($binInfo.Length) bytes"
Write-Host "  Flash Range: 0x08000000..0x080047FF"
