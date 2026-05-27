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

function Get-MdwWords {
    param(
        [string]$Text,
        [string]$Address
    )

    $addrPattern = [regex]::Escape($Address.ToLowerInvariant())
    foreach ($line in ($Text -split "`r?`n")) {
        $lower = $line.ToLowerInvariant()
        if ($lower -match "^$addrPattern\s*:\s*(.+)$") {
            return ($Matches[1].Trim() -split "\s+") | Where-Object { $_ -match "^[0-9a-fA-F]{8}$" }
        }
    }
    return @()
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
        $run = Invoke-OpenOcdOnce -OpenOcdExe $openOcdExe -Commands @(
            "init",
            "halt",
            "mdw $($symbols["g_stLowPowerRtcStatus"]) 4",
            "mdw $($symbols["s_lp_runtime"]) 3",
            "mdw $($symbols["s_ledbar"]) 9",
            "mdw $DBGMCU_CR 1",
            "resume",
            "shutdown"
        ) -TimeoutSeconds $AttachTimeoutSeconds -Index $index

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
        }
        $rows.Add($row)
        $row | Export-Csv -LiteralPath $csv -Append -NoTypeInformation -Encoding UTF8
        $row | ConvertTo-Json | Set-Content -LiteralPath $lastStatus -Encoding UTF8

        Write-Host ("[{0}] #{1} {2} rtcMode={3} rtcBlock={4} lpBlock={5} ledWin={6} dbg={7}" -f `
            $row.Time, $row.Attempt, $row.Result, $row.RtcMode, $row.RtcBlock, `
            $row.LpBlockReason, $row.LedSocWindow10ms, $row.DbgmcuCr)

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
