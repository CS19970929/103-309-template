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
$LibRoot = Join-Path $ProjectRoot "STM32F10x_StdPeriph_Lib_V3.5.0"
$UpgraderRoot = Join-Path $RepoRoot "upgrader_mcu"
$CoreRoot = Join-Path $UpgraderRoot "core"
$BoardRoot = Join-Path $UpgraderRoot "stm32f103c8"
$BuildRoot = Join-Path $UpgraderRoot "build\f103c8"
$StageRoot = Join-Path $env:TEMP "codex_upgrader_mcu_f103c8"

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
    $resolvedUpgrader = (Resolve-Path $UpgraderRoot).Path
    if (-not $resolvedBuild.StartsWith($resolvedUpgrader, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "refuse to clean unexpected directory: $resolvedBuild"
    }
    Remove-Item -LiteralPath $BuildRoot -Recurse -Force
}

if (Test-Path $StageRoot) {
    Remove-Item -LiteralPath $StageRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $BuildRoot | Out-Null
New-Item -ItemType Directory -Force -Path `
    (Join-Path $StageRoot "core"), `
    (Join-Path $StageRoot "board"), `
    (Join-Path $StageRoot "stm32\drivers"), `
    (Join-Path $StageRoot "stm32\inc"), `
    (Join-Path $StageRoot "stm32\src"), `
    (Join-Path $StageRoot "obj") | Out-Null

Copy-Item -Path (Join-Path $CoreRoot "*") -Destination (Join-Path $StageRoot "core") -Recurse -Force
Copy-Item -Path (Join-Path $BoardRoot "*") -Destination (Join-Path $StageRoot "board") -Recurse -Force
Copy-Item -Path (Join-Path $LibRoot "inc\*.h") -Destination (Join-Path $StageRoot "stm32\inc") -Force

foreach ($file in @("core_cm3.h", "stm32f10x.h", "stm32f10x_conf.h", "system_stm32f10x.h", "startup_stm32f10x_hd.s")) {
    Copy-RequiredFile (Join-Path $LibRoot "drivers\$file") (Join-Path $StageRoot "stm32\drivers")
}

foreach ($file in @("misc.c", "stm32f10x_can.c", "stm32f10x_gpio.c", "stm32f10x_rcc.c", "stm32f10x_usart.c")) {
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
        "-Icore",
        "-Iboard",
        "-Istm32\drivers",
        "-Istm32\inc"
    )

    $sources = @(
        "board\board_main.c",
        "board\board_can.c",
        "board\board_uart.c",
        "board\board_system_stm32f10x.c",
        "board\board_stm32f10x_it.c",
        "core\upg_core.c",
        "core\upg_crc16.c",
        "core\upg_feidao.c",
        "core\upg_params.c",
        "core\upg_serial.c",
        "core\upg_utils.c",
        "stm32\src\misc.c",
        "stm32\src\stm32f10x_can.c",
        "stm32\src\stm32f10x_gpio.c",
        "stm32\src\stm32f10x_rcc.c",
        "stm32\src\stm32f10x_usart.c"
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

    $axf = "obj\UPG_F103C8.axf"
    $bin = "obj\UPG_F103C8.bin"
    $map = "obj\UPG_F103C8.map"
    $linkArgs = @(
        "--cpu", "Cortex-M3",
        "--library_type=microlib",
        "--scatter", "board\UPG_F103C8.sct",
        "--map",
        "--xref",
        "--callgraph",
        "--symbols",
        "--info", "sizes",
        "--list", $map
    ) + $objects + @("-o", $axf)

    Invoke-Tool $ArmLink $linkArgs "link failed: $axf"
    Invoke-Tool $FromElf @("--bin", "-o", $bin, $axf) "fromelf failed: $bin"

    Copy-Item -LiteralPath $axf -Destination (Join-Path $BuildRoot "UPG_F103C8.axf") -Force
    Copy-Item -LiteralPath $bin -Destination (Join-Path $BuildRoot "UPG_F103C8.bin") -Force
    Copy-Item -LiteralPath $map -Destination (Join-Path $BuildRoot "UPG_F103C8.map") -Force
}
finally {
    Pop-Location
}

$binPath = Join-Path $BuildRoot "UPG_F103C8.bin"
$binInfo = Get-Item $binPath
if ($binInfo.Length -gt 0x10000) {
    throw "upgrader image too large: $($binInfo.Length) bytes, limit is 65536 bytes"
}

Write-Host "Upgrader MCU build success"
Write-Host "  AXF: $(Join-Path $BuildRoot "UPG_F103C8.axf")"
Write-Host "  BIN: $binPath"
Write-Host "  MAP: $(Join-Path $BuildRoot "UPG_F103C8.map")"
Write-Host "  BIN Size: $($binInfo.Length) bytes"
Write-Host "  Flash Range: 0x08000000..0x0800FFFF"
