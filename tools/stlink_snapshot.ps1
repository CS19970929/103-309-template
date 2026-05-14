param(
    [string]$Elf = "103 + 309\Project\Users\Objects\FD_Release.axf",
    [int]$ResetRunMs = 1500,
    [switch]$NoReset,
    [switch]$SocBreak,
    [switch]$RestFinish,
    [string]$OpenOcd = "openocd",
    [string]$Gdb = "",
    [string]$InterfaceConfig = "interface/stlink.cfg",
    [string]$TargetConfig = "target/stm32f1x.cfg",
    [int]$AdapterSpeed = 950
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

function Resolve-Executable {
    param(
        [string]$Name,
        [string]$Fallback
    )
    if ($Fallback -and (Test-Path -LiteralPath $Fallback)) {
        return (Resolve-Path -LiteralPath $Fallback).Path
    }
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    throw "Executable not found: $Name"
}

$openOcdExe = Resolve-Executable -Name $OpenOcd -Fallback ""
if ([string]::IsNullOrWhiteSpace($Gdb)) {
    $defaultGdb = "C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-gdb.exe"
    $gdbExe = Resolve-Executable -Name "arm-none-eabi-gdb" -Fallback $defaultGdb
}
else {
    $gdbExe = Resolve-Executable -Name $Gdb -Fallback $Gdb
}

if ([IO.Path]::IsPathRooted($Elf)) {
    $resolvedElf = Resolve-Path -LiteralPath $Elf
}
else {
    $resolvedElf = Resolve-Path -LiteralPath (Join-Path $RepoRoot $Elf)
}
$symbolElf = Join-Path $env:TEMP ("bms_symbols_{0}.axf" -f ([IO.Path]::GetFileNameWithoutExtension($resolvedElf.Path)))
Copy-Item -LiteralPath $resolvedElf.Path -Destination $symbolElf -Force
$symbolElfGdb = $symbolElf -replace "\\", "/"

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$gdbCmd = Join-Path $env:TEMP "bms_stlink_$stamp.gdb"
$gdbLog = Join-Path $env:TEMP "bms_stlink_$stamp.gdb.log"
$gdbOut = Join-Path $env:TEMP "bms_stlink_$stamp.gdb.out.log"
$gdbErr = Join-Path $env:TEMP "bms_stlink_$stamp.gdb.err.log"
$openOcdOut = Join-Path $env:TEMP "bms_stlink_$stamp.openocd.out.log"
$openOcdErr = Join-Path $env:TEMP "bms_stlink_$stamp.openocd.err.log"

$commands = New-Object System.Collections.Generic.List[string]
$commands.Add("set pagination off")
$commands.Add("set confirm off")
$commands.Add("set print pretty on")
$commands.Add("file `"$symbolElfGdb`"")
$commands.Add("target extended-remote :3333")
if ($NoReset) {
    $commands.Add("monitor halt")
}
else {
    $commands.Add("monitor reset run")
    $commands.Add("monitor sleep $ResetRunMs")
    $commands.Add("monitor halt")
}
$commands.Add("printf `"\n--- runtime snapshot ---\n`"")
$commands.Add("info registers pc sp lr xpsr msp psp")
$commands.Add("info symbol `$pc")
$commands.Add("bt")
$commands.Add("printf `"\n--- fault regs ---\n`"")
$commands.Add("x/1wx 0xE000ED04")
$commands.Add("x/1wx 0xE000ED24")
$commands.Add("x/1wx 0xE000ED28")
$commands.Add("x/1wx 0xE000ED2C")
$commands.Add("x/1wx 0xE000ED30")
$commands.Add("x/1wx 0xE000ED34")
$commands.Add("x/1wx 0xE000ED38")
$commands.Add("x/1wx 0xE000ED3C")
$commands.Add("printf `"\n--- app vectors ---\n`"")
$commands.Add("x/8wx 0x08004800")
$commands.Add("printf `"\n--- soc globals ---\n`"")
$commands.Add("p/x SOC_Enhance_Element")
$commands.Add("p/x g_stCellInfoReport.SocElement")
$commands.Add("p/x s_soc")

if ($SocBreak) {
    $commands.Add("hbreak SOC_IntEnhance_Ctrl")
    $commands.Add("continue")
    $commands.Add("printf `"\n--- hit SOC_IntEnhance_Ctrl ---\n`"")
    $commands.Add("info registers pc sp lr xpsr")
    $commands.Add("bt")
    $commands.Add("p/x s_soc")
    $commands.Add("delete breakpoints")
}

if ($RestFinish) {
    $commands.Add("hbreak soc_update_rest_timer")
    $commands.Add("continue")
    $commands.Add("printf `"\n--- rest timer entry ---\n`"")
    $commands.Add("p/x mode")
    $commands.Add("p/x s_soc.mode")
    $commands.Add("p/x s_soc.integrate_mode")
    $commands.Add("p/x s_soc.rest_ticks")
    $commands.Add("finish")
    $commands.Add("printf `"\n--- rest timer return ---\n`"")
    $commands.Add("p/x s_soc.mode")
    $commands.Add("p/x s_soc.integrate_mode")
    $commands.Add("p/x s_soc.rest_ticks")
    $commands.Add("p/x s_soc.stable_rest_ticks")
    $commands.Add("p/x s_soc.short_rest_ticks")
    $commands.Add("p/x s_soc.rest_ref_vmin")
    $commands.Add("p/x s_soc.rest_ref_vmax")
    $commands.Add("delete breakpoints")
}

$commands.Add("monitor resume")
$commands.Add("detach")
$commands.Add("quit")
$commands | Set-Content -LiteralPath $gdbCmd -Encoding ASCII

Push-Location $RepoRoot
$openOcd = $null
try {
    Write-Host "ST-Link snapshot"
    Write-Host "  elf:       $($resolvedElf.Path)"
    Write-Host "  symbols:   $symbolElf"
    Write-Host "  openocd:   $openOcdExe"
    Write-Host "  gdb:       $gdbExe"
    Write-Host "  gdb cmd:   $gdbCmd"

    $openOcdArgs = "-f `"$InterfaceConfig`" -f `"$TargetConfig`" -c `"transport select swd`" -c `"adapter speed $AdapterSpeed`""
    $openOcd = Start-Process -FilePath $openOcdExe `
        -ArgumentList $openOcdArgs `
        -WorkingDirectory $RepoRoot `
        -WindowStyle Hidden `
        -PassThru `
        -RedirectStandardOutput $openOcdOut `
        -RedirectStandardError $openOcdErr
    Start-Sleep -Milliseconds 1800
    if ($openOcd.HasExited) {
        Write-Host "OpenOCD exited before GDB connected."
        if (Test-Path -LiteralPath $openOcdErr) {
            Get-Content -LiteralPath $openOcdErr -Tail 80
        }
        throw "OpenOCD failed to start or connect to target"
    }

    $gdbProc = Start-Process -FilePath $gdbExe `
        -ArgumentList @("-q", "-x", $gdbCmd) `
        -WindowStyle Hidden `
        -PassThru `
        -Wait `
        -RedirectStandardOutput $gdbOut `
        -RedirectStandardError $gdbErr
    $gdbExit = $gdbProc.ExitCode
    if (Test-Path -LiteralPath $gdbLog) {
        Remove-Item -LiteralPath $gdbLog -Force
    }
    foreach ($part in @($gdbOut, $gdbErr)) {
        if (Test-Path -LiteralPath $part) {
            Get-Content -LiteralPath $part | Tee-Object -FilePath $gdbLog -Append
        }
    }
}
finally {
    if (($null -ne $openOcd) -and $openOcd.Id) {
        Stop-Process -Id $openOcd.Id -Force -ErrorAction SilentlyContinue
        Wait-Process -Id $openOcd.Id -Timeout 2 -ErrorAction SilentlyContinue
    }
    Pop-Location
}

Write-Host ""
Write-Host "Snapshot logs:"
Write-Host "  gdb:        $gdbLog"
Write-Host "  openocd out:$openOcdOut"
Write-Host "  openocd err:$openOcdErr"

exit $gdbExit
