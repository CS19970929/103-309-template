param(
    [ValidateSet("quick", "build", "probe", "flash", "full")]
    [string]$Mode = "quick",
    [ValidateSet("FD_Release", "FD_Debug")]
    [string]$Target = "FD_Release",
    [string]$PythonVersion = "3.9",
    [switch]$StrictProjectCheck,
    [switch]$Flash,
    [switch]$Probe,
    [switch]$DeepProbe,
    [string]$Port = "",
    [int]$Baud = 19200,
    [int]$Slave = 1,
    [int]$OnlineSamples = 10,
    [double]$OnlineInterval = 0.5,
    [string]$Uv4 = "C:\Keil_v5\UV4\UV4.exe"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Project = Join-Path $RepoRoot "103 + 309\Project\Users\CommomSH367309_16series_103RCT6_C.uvprojx"
$ProjectUsers = Join-Path $RepoRoot "103 + 309\Project\Users"
$Objects = Join-Path $ProjectUsers "Objects"

function Invoke-LoggedStep {
    param(
        [string]$Name,
        [scriptblock]$Action,
        [switch]$AllowFailure
    )
    Write-Host ""
    Write-Host "============================================================"
    Write-Host $Name
    Write-Host "============================================================"
    $sw = [Diagnostics.Stopwatch]::StartNew()
    try {
        & $Action
        $exitCode = if ($LASTEXITCODE -ne $null) { $LASTEXITCODE } else { 0 }
        if ($exitCode -ne 0 -and -not $AllowFailure) {
            throw "$Name failed with exit code $exitCode"
        }
        if ($exitCode -ne 0) {
            Write-Warning "$Name returned exit code $exitCode"
        }
    }
    finally {
        $sw.Stop()
        Write-Host ("Elapsed: {0}" -f $sw.Elapsed)
    }
}

function Invoke-Python {
    param([string[]]$PyArgs)
    $py = (Get-Command py.exe -ErrorAction Stop).Source
    & $py "-$PythonVersion" @PyArgs
}

function Invoke-KeilBuild {
    param([string]$BuildTarget)
    if (-not (Test-Path -LiteralPath $Uv4)) {
        throw "UV4 not found: $Uv4"
    }
    $log = Join-Path $env:TEMP ("bms_{0}_{1}.log" -f $BuildTarget, (Get-Date -Format "yyyyMMdd_HHmmss"))
    if (Test-Path -LiteralPath $log) {
        Remove-Item -LiteralPath $log -Force
    }
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Uv4
    $psi.WorkingDirectory = $ProjectUsers
    $psi.Arguments = "-j0 -b `"$Project`" -t `"$BuildTarget`" -o `"$log`""
    $psi.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
    $psi.UseShellExecute = $true
    $p = [Diagnostics.Process]::Start($psi)
    if (-not $p.WaitForExit(180000)) {
        $id = $p.Id
        try { $p.Kill() } catch {}
        throw "Keil build timeout, pid=$id"
    }
    Write-Host "Keil exit code: $($p.ExitCode)"
    Write-Host "Keil log:       $log"
    if (-not (Test-Path -LiteralPath $log)) {
        throw "Keil log missing: $log"
    }
    $important = Select-String -Path $log -Pattern "^Program Size|0 Error\(s\)|[1-9][0-9]* Error\(s\)|Warning\(s\)|After Build|Build Time|warning:" |
        Select-Object -Last 120
    $important | ForEach-Object { $_.Line }
    $text = Get-Content -LiteralPath $log -Raw
    if ($text -notmatch "0 Error\(s\)") {
        throw "Keil build did not report 0 Error(s)"
    }
}

function Get-ArtifactPath {
    param([string]$BuildTarget, [string]$Ext)
    return Join-Path $Objects ("{0}.{1}" -f $BuildTarget, $Ext)
}

Push-Location $RepoRoot
try {
    Write-Host "BMS dev workflow"
    Write-Host "  mode:   $Mode"
    Write-Host "  target: $Target"
    Write-Host "  root:   $RepoRoot"

    if ($Mode -eq "quick" -or $Mode -eq "full") {
        Invoke-LoggedStep "Project consistency check" {
            Invoke-Python -PyArgs @("tools\project_check.py", "-q")
        } -AllowFailure:(!$StrictProjectCheck)

        Invoke-LoggedStep "SOC Python replay tests" {
            Invoke-Python -PyArgs @("tools\soc_replay_test.py")
        }

        Invoke-LoggedStep "SOC host C tests" {
            Invoke-Python -PyArgs @("tools\run_soc_host_c_test.py")
        }
    }

    if ($Mode -eq "build" -or $Mode -eq "flash" -or $Mode -eq "full") {
        Invoke-LoggedStep "Keil build $Target" {
            Invoke-KeilBuild -BuildTarget $Target
        }
    }

    if ($Mode -eq "flash" -or ($Mode -eq "full" -and $Flash)) {
        $bin = Get-ArtifactPath -BuildTarget $Target -Ext "bin"
        Invoke-LoggedStep "Safe app flash at 0x08004800" {
            & powershell -ExecutionPolicy Bypass -File "tools\soc_flash_app_safe.ps1" -Bin $bin -Flash
            if ($LASTEXITCODE -ne 0) {
                throw "safe flash failed with exit code $LASTEXITCODE"
            }
        }
    }

    if ($Mode -eq "probe" -or ($Mode -eq "full" -and $Probe) -or ($Mode -eq "flash" -and $Probe)) {
        $elf = Get-ArtifactPath -BuildTarget $Target -Ext "axf"
        Invoke-LoggedStep "ST-Link runtime snapshot" {
            $probeArgs = @(
                "-ExecutionPolicy", "Bypass",
                "-File", "tools\stlink_snapshot.ps1",
                "-Elf", $elf
            )
            if ($DeepProbe) {
                $probeArgs += @("-SocBreak", "-RestFinish")
            }
            & powershell @probeArgs
            if ($LASTEXITCODE -ne 0) {
                throw "ST-Link snapshot failed with exit code $LASTEXITCODE"
            }
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($Port)) {
        $csv = "SOC_ONLINE_MONITOR_{0}.csv" -f (Get-Date -Format "yyyyMMdd_HHmmss")
        Invoke-LoggedStep "SOC online monitor $Port@$Baud" {
            Invoke-Python -PyArgs @(
                "tools\soc_online_monitor.py",
                "--port", $Port,
                "--baud", [string]$Baud,
                "--slave", [string]$Slave,
                "--samples", [string]$OnlineSamples,
                "--interval", [string]$OnlineInterval,
                "--csv", $csv
            )
        }
    }

    Write-Host ""
    Write-Host "Workflow completed."
}
finally {
    Pop-Location
}
