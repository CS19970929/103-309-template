param(
    [ValidateSet("ReleaseProxy", "DebugProbe")]
    [string]$Mode = "ReleaseProxy",
    [string]$Elf = "103 + 309\Project\Users\Objects\FD_Release.axf",
    [int]$IntervalSeconds = 10,
    [int]$Count = 0,
    [int]$DurationMinutes = 0,
    [string]$LogDir = "logs\stlink_bms_monitor",
    [string]$OpenOcd = "",
    [string]$Nm = "",
    [string]$InterfaceConfig = "interface/stlink.cfg",
    [string]$TargetConfig = "target/stm32f1x.cfg",
    [int]$AdapterSpeed = 1800,
    [int]$AttachTimeoutSeconds = 8,
    [int]$DebugSettleMs = 1500,
    [int]$DebugPrepareAttempts = 12,
    [int]$DebugPrepareIntervalSeconds = 2,
    [switch]$KeepRawLogs
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$ResolvedLogDir = if ([IO.Path]::IsPathRooted($LogDir)) {
    $LogDir
}
else {
    Join-Path $RepoRoot $LogDir
}
$RawLogDir = Join-Path $ResolvedLogDir "raw"
New-Item -ItemType Directory -Force -Path $ResolvedLogDir | Out-Null
New-Item -ItemType Directory -Force -Path $RawLogDir | Out-Null

$DBGMCU_CR = "0xE0042004"
$LOW_POWER_DEBUG_MASK = "0x00000307"

function Resolve-Tool {
    param(
        [string]$Explicit,
        [string]$Name,
        [string[]]$Fallbacks
    )

    if (-not [string]::IsNullOrWhiteSpace($Explicit)) {
        if (Test-Path -LiteralPath $Explicit) {
            return (Resolve-Path -LiteralPath $Explicit).Path
        }
        $cmd = Get-Command $Explicit -ErrorAction SilentlyContinue
        if ($cmd) {
            return $cmd.Source
        }
        throw "Executable not found: $Explicit"
    }

    $found = Get-Command $Name -ErrorAction SilentlyContinue
    if ($found) {
        return $found.Source
    }

    foreach ($path in $Fallbacks) {
        if (Test-Path -LiteralPath $path) {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }

    throw "Executable not found: $Name"
}

function Resolve-RepoPath {
    param([string]$Path)
    if ([IO.Path]::IsPathRooted($Path)) {
        return (Resolve-Path -LiteralPath $Path).Path
    }
    return (Resolve-Path -LiteralPath (Join-Path $RepoRoot $Path)).Path
}

function Get-SymbolTable {
    param(
        [string]$NmExe,
        [string]$ElfPath
    )

    $symbols = @{}
    & $NmExe -n $ElfPath 2>$null | ForEach-Object {
        if ($_ -match "^([0-9a-fA-F]+)\s+\S\s+(.+)$") {
            $symbols[$Matches[2].Trim()] = "0x$($Matches[1])"
        }
    }
    return $symbols
}

function Invoke-OpenOcdOnce {
    param(
        [string]$OpenOcdExe,
        [string[]]$Commands,
        [int]$TimeoutSeconds,
        [int]$Index
    )

    $stamp = Get-Date -Format "yyyyMMdd_HHmmss_fff"
    $stdout = Join-Path $RawLogDir ("openocd_{0}_{1:0000}.out.log" -f $stamp, $Index)
    $stderr = Join-Path $RawLogDir ("openocd_{0}_{1:0000}.err.log" -f $stamp, $Index)

    $args = @(
        "-f `"$InterfaceConfig`"",
        "-f `"$TargetConfig`"",
        "-c `"transport select swd`"",
        "-c `"adapter speed $AdapterSpeed`""
    )
    foreach ($cmd in $Commands) {
        $args += "-c `"$cmd`""
    }
    $argLine = $args -join " "

    $proc = Start-Process -FilePath $OpenOcdExe `
        -ArgumentList $argLine `
        -WorkingDirectory $RepoRoot `
        -WindowStyle Hidden `
        -PassThru `
        -RedirectStandardOutput $stdout `
        -RedirectStandardError $stderr

    $exited = $proc.WaitForExit($TimeoutSeconds * 1000)
    $timedOut = $false
    if (-not $exited) {
        $timedOut = $true
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        Wait-Process -Id $proc.Id -Timeout 2 -ErrorAction SilentlyContinue
    }

    $outText = if (Test-Path -LiteralPath $stdout) { Get-Content -LiteralPath $stdout -Raw } else { "" }
    $errText = if (Test-Path -LiteralPath $stderr) { Get-Content -LiteralPath $stderr -Raw } else { "" }
    $text = ($outText + "`n" + $errText)

    if (-not $KeepRawLogs) {
        Remove-Item -LiteralPath $stdout, $stderr -Force -ErrorAction SilentlyContinue
    }

    return [pscustomobject]@{
        ExitCode = if ($timedOut) { -999 } else { $proc.ExitCode }
        TimedOut = $timedOut
        Text = $text
    }
}

function Get-MemValues {
    param(
        [string]$Text,
        [string]$Address,
        [int]$HexDigits
    )

    $addrPattern = [regex]::Escape($Address.ToLowerInvariant())
    $valuePattern = "^[0-9a-fA-F]{$HexDigits}$"
    foreach ($line in ($Text -split "`r?`n")) {
        $lower = $line.ToLowerInvariant()
        if ($lower -match "^$addrPattern\s*:\s*(.+)$") {
            return ($Matches[1].Trim() -split "\s+") | Where-Object { $_ -match $valuePattern }
        }
    }
    return @()
}

function Get-MdwWords {
    param(
        [string]$Text,
        [string]$Address
    )
    return Get-MemValues -Text $Text -Address $Address -HexDigits 8
}

function Get-MdhWords {
    param(
        [string]$Text,
        [string]$Address
    )
    return Get-MemValues -Text $Text -Address $Address -HexDigits 4
}

function Convert-HexWord {
    param([string]$Word)
    if ([string]::IsNullOrWhiteSpace($Word)) {
        return $null
    }
    return [Convert]::ToUInt32($Word, 16)
}

function Get-WordAt {
    param(
        [object[]]$Words,
        [int]$Index
    )
    if (($null -eq $Words) -or ($Words.Count -le $Index)) {
        return ""
    }
    return [string]$Words[$Index]
}

function Decode-Sample {
    param(
        [string]$Text,
        [hashtable]$Symbols
    )

    $rtcWords = Get-MdwWords -Text $Text -Address $Symbols["g_stLowPowerRtcStatus"]
    $lpWords = Get-MdwWords -Text $Text -Address $Symbols["s_lp_runtime"]
    $ledWords = Get-MdwWords -Text $Text -Address $Symbols["s_ledbar"]
    $dbgWords = Get-MdwWords -Text $Text -Address $DBGMCU_CR
    $mcuWords = Get-MdwWords -Text $Text -Address "0xE0042000"
    $rccWords = Get-MdwWords -Text $Text -Address "0x40021000"
    $pwrWords = Get-MdwWords -Text $Text -Address "0x40007000"
    $faultWords = Get-MdwWords -Text $Text -Address "0xE000ED04"
    $faultStatusWords = Get-MdwWords -Text $Text -Address "0xE000ED24"
    $cellHalf = if ($Symbols.ContainsKey("g_stCellInfoReport")) { Get-MdhWords -Text $Text -Address $Symbols["g_stCellInfoReport"] } else { @() }
    $otherHalf = if ($Symbols.ContainsKey("OtherElement")) { Get-MdhWords -Text $Text -Address $Symbols["OtherElement"] } else { @() }
    $typeCWords = if ($Symbols.ContainsKey("g_u16TypeCOutCurrent_mA")) { Get-MdwWords -Text $Text -Address $Symbols["g_u16TypeCOutCurrent_mA"] } else { @() }
    $vbatWords = if ($Symbols.ContainsKey("g_u32Vbat_mV")) { Get-MdwWords -Text $Text -Address $Symbols["g_u32Vbat_mV"] } else { @() }
    $afeSeqWords = if ($Symbols.ContainsKey("g_u32AfeCurrentSampleSeq")) { Get-MdwWords -Text $Text -Address $Symbols["g_u32AfeCurrentSampleSeq"] } else { @() }
    $flashWords = if ($Symbols.ContainsKey("u8FlashUpdateFlag")) { Get-MdwWords -Text $Text -Address $Symbols["u8FlashUpdateFlag"] } else { @() }
    $factoryWords = if ($Symbols.ContainsKey("s_u8FactoryAgingState")) { Get-MdwWords -Text $Text -Address $Symbols["s_u8FactoryAgingState"] } else { @() }
    $sysTimeWords = if ($Symbols.ContainsKey("sys_time")) { Get-MdwWords -Text $Text -Address $Symbols["sys_time"] } else { @() }
    $socEnhanceWords = if ($Symbols.ContainsKey("SOC_Enhance_Element")) { Get-MdwWords -Text $Text -Address $Symbols["SOC_Enhance_Element"] } else { @() }
    $systemErrWords = if ($Symbols.ContainsKey("System_ErrFlag")) { Get-MdwWords -Text $Text -Address $Symbols["System_ErrFlag"] } else { @() }

    $rtc0 = Convert-HexWord (Get-WordAt $rtcWords 0)
    $rtc1 = Convert-HexWord (Get-WordAt $rtcWords 1)
    $rtc2 = Convert-HexWord (Get-WordAt $rtcWords 2)
    $lp0 = Convert-HexWord (Get-WordAt $lpWords 0)
    $lp1 = Convert-HexWord (Get-WordAt $lpWords 1)
    $lp2 = Convert-HexWord (Get-WordAt $lpWords 2)
    $led0 = Convert-HexWord (Get-WordAt $ledWords 0)
    $led2 = Convert-HexWord (Get-WordAt $ledWords 2)
    $led3 = Convert-HexWord (Get-WordAt $ledWords 3)
    $led4 = Convert-HexWord (Get-WordAt $ledWords 4)
    $mcuId = Convert-HexWord (Get-WordAt $mcuWords 0)
    $rccCr = Convert-HexWord (Get-WordAt $rccWords 0)
    $rccCfgr = Convert-HexWord (Get-WordAt $rccWords 1)
    $pwrCr = Convert-HexWord (Get-WordAt $pwrWords 0)
    $pwrCsr = Convert-HexWord (Get-WordAt $pwrWords 1)
    $typeCWord = Convert-HexWord (Get-WordAt $typeCWords 0)
    $flashWord = Convert-HexWord (Get-WordAt $flashWords 0)
    $factoryWord = Convert-HexWord (Get-WordAt $factoryWords 0)

    $rtcMode = if ($null -ne $rtc0) { $rtc0 -band 0xff } else { $null }
    $rtcReady = if ($null -ne $rtc0) { ($rtc0 -shr 8) -band 0xff } else { $null }
    $rtcBlock = if ($null -ne $rtc0) { ($rtc0 -shr 16) -band 0xff } else { $null }
    $rtcWake = if ($null -ne $rtc0) { ($rtc0 -shr 24) -band 0xff } else { $null }
    $rtcDelay = if ($null -ne $rtc1) { $rtc1 -band 0xffff } else { $null }
    $rtcTarget = if ($null -ne $rtc1) { ($rtc1 -shr 16) -band 0xffff } else { $null }

    return [pscustomobject]@{
        RtcMode = $rtcMode
        RtcReady = $rtcReady
        RtcBlock = $rtcBlock
        RtcWake = $rtcWake
        RtcDelaySeconds = $rtcDelay
        RtcTargetSeconds = $rtcTarget
        RtcElapsedSeconds = $rtc2
        LpState = $lp0
        LpBlockReason = if ($null -ne $lp1) { ("0x{0:x8}" -f $lp1) } else { "" }
        LpLastSleepSeconds = $lp2
        LedInitialized = if ($null -ne $led0) { $led0 -band 0xff } else { $null }
        LedSleep = if ($null -ne $led0) { ($led0 -shr 8) -band 0xff } else { $null }
        LedBlank = if ($null -ne $led0) { ($led0 -shr 16) -band 0xff } else { $null }
        LedNumber = if ($null -ne $led0) { ($led0 -shr 24) -band 0xff } else { $null }
        LedFrameMask = if ($null -ne $led2) { ("0x{0:x8}" -f $led2) } else { "" }
        LedScanEnabled = if ($null -ne $led3) { ($led3 -shr 16) -band 0xff } else { $null }
        LedSocWindow10ms = if ($null -ne $led4) { $led4 -band 0xffff } else { $null }
        LedStartupArmed = if ($null -ne $led4) { ($led4 -shr 16) -band 0xff } else { $null }
        DbgmcuCr = if (($null -ne $dbgWords) -and ($dbgWords.Count -gt 0)) { "0x$($dbgWords[0])" } else { "" }
        McuDevId = if ($null -ne $mcuId) { ("0x{0:x3}" -f ($mcuId -band 0xfff)) } else { "" }
        McuRevId = if ($null -ne $mcuId) { ("0x{0:x4}" -f (($mcuId -shr 16) -band 0xffff)) } else { "" }
        RccCr = if ($null -ne $rccCr) { ("0x{0:x8}" -f $rccCr) } else { "" }
        RccCfgr = if ($null -ne $rccCfgr) { ("0x{0:x8}" -f $rccCfgr) } else { "" }
        PwrCr = if ($null -ne $pwrCr) { ("0x{0:x8}" -f $pwrCr) } else { "" }
        PwrCsr = if ($null -ne $pwrCsr) { ("0x{0:x8}" -f $pwrCsr) } else { "" }
        ScbIcsr = "0x$(Get-WordAt $faultWords 0)"
        ScbCfsr = "0x$(Get-WordAt $faultStatusWords 1)"
        ScbHfsr = "0x$(Get-WordAt $faultStatusWords 2)"
        CellVMax_mV = Convert-HexWord (Get-WordAt $cellHalf 32)
        CellVMin_mV = Convert-HexWord (Get-WordAt $cellHalf 33)
        CellVDelta_mV = Convert-HexWord (Get-WordAt $cellHalf 36)
        PackVoltage_10mV = Convert-HexWord (Get-WordAt $cellHalf 37)
        TempMax_Plus40C_x10 = Convert-HexWord (Get-WordAt $cellHalf 48)
        TempMin_Plus40C_x10 = Convert-HexWord (Get-WordAt $cellHalf 49)
        Ichg_A10 = Convert-HexWord (Get-WordAt $cellHalf 50)
        Idsg_A10 = Convert-HexWord (Get-WordAt $cellHalf 51)
        SocPercent = Convert-HexWord (Get-WordAt $cellHalf 52)
        SohPercent = Convert-HexWord (Get-WordAt $cellHalf 53)
        CapacityNow_Ah100 = Convert-HexWord (Get-WordAt $cellHalf 54)
        CapacityFull_Ah100 = Convert-HexWord (Get-WordAt $cellHalf 55)
        CapacityFactory_Ah100 = Convert-HexWord (Get-WordAt $cellHalf 56)
        CycleTimes = Convert-HexWord (Get-WordAt $cellHalf 57)
        FaultThird = if ($cellHalf.Count -gt 60) { "0x$(Get-WordAt $cellHalf 60)" } else { "" }
        TypeCOutCurrent_mA = if ($null -ne $typeCWord) { $typeCWord -band 0xffff } else { $null }
        Vbat_mV = Convert-HexWord (Get-WordAt $vbatWords 0)
        AfeSampleSeq = Convert-HexWord (Get-WordAt $afeSeqWords 0)
        FlashUpdateFlag = if ($null -ne $flashWord) { $flashWord -band 0xff } else { $null }
        FlashUpdateE2prom = if ($null -ne $flashWord) { ($flashWord -shr 8) -band 0xff } else { $null }
        FactoryAgingState = if ($null -ne $factoryWord) { $factoryWord -band 0xff } else { $null }
        SleepVNormal_mV = Convert-HexWord (Get-WordAt $otherHalf 16)
        SleepTimeNormal_min = Convert-HexWord (Get-WordAt $otherHalf 17)
        SleepVLow_mV = Convert-HexWord (Get-WordAt $otherHalf 18)
        SleepTimeVLow_min = Convert-HexWord (Get-WordAt $otherHalf 19)
        SleepRtcWakeTime_min = Convert-HexWord (Get-WordAt $otherHalf 22)
        SleepTimeRtc_min = Convert-HexWord (Get-WordAt $otherHalf 23)
        ConfigSeriesNum = Convert-HexWord (Get-WordAt $otherHalf 28)
        ConfigCsRes_mOhm = Convert-HexWord (Get-WordAt $otherHalf 29)
        ConfigCsResNum = Convert-HexWord (Get-WordAt $otherHalf 30)
        SysTimeRaw0 = "0x$(Get-WordAt $sysTimeWords 0)"
        SocEnhanceRaw0 = "0x$(Get-WordAt $socEnhanceWords 0)"
        SystemErrRaw0 = "0x$(Get-WordAt $systemErrWords 0)"
    }
}

function Get-ResultKind {
    param(
        [object]$Run,
        [object]$Decoded
    )

    if ($Run.TimedOut) {
        return "TIMEOUT_LOW_POWER_OR_DBG_OFF"
    }
    if (($Run.ExitCode -eq 0) -and ($Run.Text -match "Examination succeed")) {
        if ($Decoded.DbgmcuCr -eq "0x00000000") {
            return "ATTACH_OK_RUN_OR_WAKE"
        }
        return "ATTACH_OK_DEBUG_HOLD"
    }
    if ($Run.Text -match "unable to connect to the target") {
        return "LOW_POWER_OR_DBG_OFF"
    }
    if ($Run.Text -match "Fail reading CTRL/STAT") {
        return "SWD_UNRESPONSIVE"
    }
    if ($Run.Text -match "Could not find MEM-AP") {
        return "LOW_POWER_MEMAP_OFF"
    }
    return "ATTACH_FAIL"
}

$openOcdExe = Resolve-Tool -Explicit $OpenOcd -Name "openocd" -Fallbacks @(
    "C:\Users\Administrator\AppData\Local\Microsoft\WinGet\Packages\xpack-dev-tools.openocd-xpack_Microsoft.Winget.Source_8wekyb3d8bbwe\xpack-openocd-0.12.0-7\bin\openocd.exe"
)
$nmExe = Resolve-Tool -Explicit $Nm -Name "arm-none-eabi-nm" -Fallbacks @(
    "C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-nm.exe"
)
$elfPath = Resolve-RepoPath $Elf
$symbols = Get-SymbolTable -NmExe $nmExe -ElfPath $elfPath
foreach ($required in @("g_stLowPowerRtcStatus", "s_lp_runtime", "s_ledbar")) {
    if (-not $symbols.ContainsKey($required)) {
        throw "Required symbol not found in ELF: $required"
    }
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$csv = Join-Path $ResolvedLogDir ("stlink_bms_monitor_{0}.csv" -f $stamp)
$summary = Join-Path $ResolvedLogDir ("stlink_bms_monitor_{0}.summary.txt" -f $stamp)
$lastStatus = Join-Path $ResolvedLogDir "last_status.json"

Write-Host "ST-Link BMS monitor"
Write-Host "  mode:      $Mode"
Write-Host "  elf:       $elfPath"
Write-Host "  openocd:   $openOcdExe"
Write-Host "  nm:        $nmExe"
Write-Host "  interval:  $IntervalSeconds s"
Write-Host "  count:     $Count (0 means until DurationMinutes or Ctrl+C)"
Write-Host "  duration:  $DurationMinutes min (0 means no duration limit)"
Write-Host "  csv:       $csv"
Write-Host "  symbols:   rtc=$($symbols["g_stLowPowerRtcStatus"]) lp=$($symbols["s_lp_runtime"]) led=$($symbols["s_ledbar"])"

if ($Mode -eq "DebugProbe") {
    Write-Warning "DebugProbe will set DBGMCU low-power debug bits. This is for logic monitoring only, not current measurement."
    $prepared = $false
    for ($prepIndex = 1; $prepIndex -le $DebugPrepareAttempts; ++$prepIndex) {
        Write-Host ("DebugProbe prepare attempt {0}/{1}" -f $prepIndex, $DebugPrepareAttempts)
        $prepare = Invoke-OpenOcdOnce -OpenOcdExe $openOcdExe -Commands @(
            "init",
            "reset run",
            "sleep $DebugSettleMs",
            "halt",
            "mww $DBGMCU_CR $LOW_POWER_DEBUG_MASK",
            "mdw $DBGMCU_CR 1",
            "resume",
            "shutdown"
        ) -TimeoutSeconds $AttachTimeoutSeconds -Index (-$prepIndex)
        if (($prepare.ExitCode -eq 0) -and ($prepare.Text -match "Examination succeed")) {
            $prepared = $true
            break
        }
        Start-Sleep -Seconds $DebugPrepareIntervalSeconds
    }
    if (-not $prepared) {
        Write-Warning "Failed to prepare DebugProbe. If the board is already in Release STOP with DBG_STOP cleared, press wake/reset or use ReleaseProxy until the next wake window."
    }
}

$rows = New-Object System.Collections.Generic.List[object]
$deadline = if ($DurationMinutes -gt 0) { (Get-Date).AddMinutes($DurationMinutes) } else { $null }
$index = 1

try {
    while ($true) {
        $now = Get-Date
        $sampleCommands = New-Object System.Collections.Generic.List[string]
        $sampleCommands.Add("init")
        $sampleCommands.Add("halt")
        $sampleCommands.Add("mdw $($symbols["g_stLowPowerRtcStatus"]) 4")
        $sampleCommands.Add("mdw $($symbols["s_lp_runtime"]) 3")
        $sampleCommands.Add("mdw $($symbols["s_ledbar"]) 9")
        $sampleCommands.Add("mdw $DBGMCU_CR 1")
        $sampleCommands.Add("mdw 0xE0042000 2")
        $sampleCommands.Add("mdw 0x40021000 2")
        $sampleCommands.Add("mdw 0x40007000 2")
        $sampleCommands.Add("mdw 0xE000ED04 1")
        $sampleCommands.Add("mdw 0xE000ED24 4")
        if ($symbols.ContainsKey("g_stCellInfoReport")) { $sampleCommands.Add("mdh $($symbols["g_stCellInfoReport"]) 64") }
        if ($symbols.ContainsKey("OtherElement")) { $sampleCommands.Add("mdh $($symbols["OtherElement"]) 32") }
        if ($symbols.ContainsKey("g_u16TypeCOutCurrent_mA")) { $sampleCommands.Add("mdw $($symbols["g_u16TypeCOutCurrent_mA"]) 1") }
        if ($symbols.ContainsKey("g_u32Vbat_mV")) { $sampleCommands.Add("mdw $($symbols["g_u32Vbat_mV"]) 1") }
        if ($symbols.ContainsKey("g_u32AfeCurrentSampleSeq")) { $sampleCommands.Add("mdw $($symbols["g_u32AfeCurrentSampleSeq"]) 1") }
        if ($symbols.ContainsKey("u8FlashUpdateFlag")) { $sampleCommands.Add("mdw $($symbols["u8FlashUpdateFlag"]) 1") }
        if ($symbols.ContainsKey("s_u8FactoryAgingState")) { $sampleCommands.Add("mdw $($symbols["s_u8FactoryAgingState"]) 1") }
        if ($symbols.ContainsKey("sys_time")) { $sampleCommands.Add("mdw $($symbols["sys_time"]) 18") }
        if ($symbols.ContainsKey("SOC_Enhance_Element")) { $sampleCommands.Add("mdw $($symbols["SOC_Enhance_Element"]) 12") }
        if ($symbols.ContainsKey("System_ErrFlag")) { $sampleCommands.Add("mdw $($symbols["System_ErrFlag"]) 6") }
        $sampleCommands.Add("resume")
        $sampleCommands.Add("shutdown")

        $run = Invoke-OpenOcdOnce -OpenOcdExe $openOcdExe -Commands $sampleCommands.ToArray() -TimeoutSeconds $AttachTimeoutSeconds -Index $index

        $decoded = Decode-Sample -Text $run.Text -Symbols $symbols
        $kind = Get-ResultKind -Run $run -Decoded $decoded
        $row = [pscustomobject]@{
            Time = $now.ToString("yyyy-MM-dd HH:mm:ss")
            Attempt = $index
            Mode = $Mode
            Result = $kind
            ExitCode = $run.ExitCode
            RtcMode = $decoded.RtcMode
            RtcReady = $decoded.RtcReady
            RtcBlock = $decoded.RtcBlock
            RtcWake = $decoded.RtcWake
            RtcDelaySeconds = $decoded.RtcDelaySeconds
            RtcTargetSeconds = $decoded.RtcTargetSeconds
            RtcElapsedSeconds = $decoded.RtcElapsedSeconds
            LpState = $decoded.LpState
            LpBlockReason = $decoded.LpBlockReason
            LpLastSleepSeconds = $decoded.LpLastSleepSeconds
            LedSleep = $decoded.LedSleep
            LedBlank = $decoded.LedBlank
            LedNumber = $decoded.LedNumber
            LedSocWindow10ms = $decoded.LedSocWindow10ms
            LedStartupArmed = $decoded.LedStartupArmed
            LedFrameMask = $decoded.LedFrameMask
            LedScanEnabled = $decoded.LedScanEnabled
            DbgmcuCr = $decoded.DbgmcuCr
            McuDevId = $decoded.McuDevId
            McuRevId = $decoded.McuRevId
            RccCr = $decoded.RccCr
            RccCfgr = $decoded.RccCfgr
            PwrCr = $decoded.PwrCr
            PwrCsr = $decoded.PwrCsr
            ScbIcsr = $decoded.ScbIcsr
            ScbCfsr = $decoded.ScbCfsr
            ScbHfsr = $decoded.ScbHfsr
            CellVMax_mV = $decoded.CellVMax_mV
            CellVMin_mV = $decoded.CellVMin_mV
            CellVDelta_mV = $decoded.CellVDelta_mV
            PackVoltage_10mV = $decoded.PackVoltage_10mV
            TempMax_Plus40C_x10 = $decoded.TempMax_Plus40C_x10
            TempMin_Plus40C_x10 = $decoded.TempMin_Plus40C_x10
            Ichg_A10 = $decoded.Ichg_A10
            Idsg_A10 = $decoded.Idsg_A10
            SocPercent = $decoded.SocPercent
            SohPercent = $decoded.SohPercent
            CapacityNow_Ah100 = $decoded.CapacityNow_Ah100
            CapacityFull_Ah100 = $decoded.CapacityFull_Ah100
            CapacityFactory_Ah100 = $decoded.CapacityFactory_Ah100
            CycleTimes = $decoded.CycleTimes
            FaultThird = $decoded.FaultThird
            TypeCOutCurrent_mA = $decoded.TypeCOutCurrent_mA
            Vbat_mV = $decoded.Vbat_mV
            AfeSampleSeq = $decoded.AfeSampleSeq
            FlashUpdateFlag = $decoded.FlashUpdateFlag
            FlashUpdateE2prom = $decoded.FlashUpdateE2prom
            FactoryAgingState = $decoded.FactoryAgingState
            SleepVNormal_mV = $decoded.SleepVNormal_mV
            SleepTimeNormal_min = $decoded.SleepTimeNormal_min
            SleepVLow_mV = $decoded.SleepVLow_mV
            SleepTimeVLow_min = $decoded.SleepTimeVLow_min
            SleepRtcWakeTime_min = $decoded.SleepRtcWakeTime_min
            SleepTimeRtc_min = $decoded.SleepTimeRtc_min
            ConfigSeriesNum = $decoded.ConfigSeriesNum
            ConfigCsRes_mOhm = $decoded.ConfigCsRes_mOhm
            ConfigCsResNum = $decoded.ConfigCsResNum
            SysTimeRaw0 = $decoded.SysTimeRaw0
            SocEnhanceRaw0 = $decoded.SocEnhanceRaw0
            SystemErrRaw0 = $decoded.SystemErrRaw0
        }
        $rows.Add($row)
        $row | Export-Csv -LiteralPath $csv -Append -NoTypeInformation -Encoding UTF8
        $row | ConvertTo-Json | Set-Content -LiteralPath $lastStatus -Encoding UTF8

        Write-Host ("[{0}] #{1} {2} rtcMode={3} rtcBlock={4} lpBlock={5} ledWin={6} soc={7} vmin={8} vmax={9} dbg={10}" -f `
            $row.Time, $row.Attempt, $row.Result, $row.RtcMode, $row.RtcBlock, `
            $row.LpBlockReason, $row.LedSocWindow10ms, $row.SocPercent, `
            $row.CellVMin_mV, $row.CellVMax_mV, $row.DbgmcuCr)

        if (($Count -gt 0) -and ($index -ge $Count)) {
            break
        }
        if (($null -ne $deadline) -and ((Get-Date) -ge $deadline)) {
            break
        }
        ++$index
        Start-Sleep -Seconds $IntervalSeconds
    }
}
finally {
    $total = $rows.Count
    $ok = @($rows | Where-Object { $_.Result -like "ATTACH_OK*" }).Count
    $low = @($rows | Where-Object { $_.Result -in @("LOW_POWER_OR_DBG_OFF", "SWD_UNRESPONSIVE", "LOW_POWER_MEMAP_OFF", "TIMEOUT_LOW_POWER_OR_DBG_OFF") }).Count
    $fail = $total - $ok - $low
    $lines = @(
        "ST-Link BMS monitor summary",
        "time=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
        "mode=$Mode",
        "csv=$csv",
        "total=$total",
        "attach_ok=$ok",
        "low_power_or_dbg_off=$low",
        "other_fail=$fail"
    )
    $lines | Set-Content -LiteralPath $summary -Encoding UTF8
    Write-Host ""
    $lines | ForEach-Object { Write-Host $_ }
}
