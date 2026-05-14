param(
    [ValidateSet("quick", "probe", "deep-probe")]
    [string]$Mode = "quick",
    [ValidateSet("FD_Release", "FD_Debug")]
    [string]$Target = "FD_Release",
    [int]$IntervalSeconds = 300,
    [int]$Count = 0,
    [string]$LogDir = "logs\bms_watch"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$ResolvedLogDir = Join-Path $RepoRoot $LogDir
New-Item -ItemType Directory -Force -Path $ResolvedLogDir | Out-Null

function Invoke-WatchRun {
    param([int]$Index)
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $log = Join-Path $ResolvedLogDir ("{0}_{1:0000}.log" -f $stamp, $Index)
    $status = Join-Path $ResolvedLogDir "last_status.json"

    $workflowMode = if ($Mode -eq "quick") { "quick" } else { "probe" }
    $args = @(
        "-ExecutionPolicy", "Bypass",
        "-File", "tools\bms_dev_workflow.ps1",
        "-Mode", $workflowMode,
        "-Target", $Target
    )
    if ($Mode -eq "deep-probe") {
        $args += "-DeepProbe"
    }

    Write-Host ""
    Write-Host "[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] run #$Index mode=$Mode target=$Target"
    Write-Host "log: $log"
    & powershell @args 2>&1 | Tee-Object -FilePath $log
    $exitCode = $LASTEXITCODE

    [pscustomobject]@{
        time = (Get-Date).ToString("s")
        mode = $Mode
        target = $Target
        index = $Index
        exit_code = $exitCode
        log = $log
    } | ConvertTo-Json | Set-Content -LiteralPath $status -Encoding UTF8

    if ($exitCode -eq 0) {
        Write-Host "run #$Index passed"
    }
    else {
        Write-Warning "run #$Index failed with exit code $exitCode"
    }
}

Push-Location $RepoRoot
try {
    Write-Host "BMS local watch"
    Write-Host "  mode:     $Mode"
    Write-Host "  target:   $Target"
    Write-Host "  interval: $IntervalSeconds s"
    Write-Host "  count:    $Count (0 means forever)"
    Write-Host "  log dir:  $ResolvedLogDir"

    $index = 1
    while ($true) {
        Invoke-WatchRun -Index $index
        if ($Count -gt 0 -and $index -ge $Count) {
            break
        }
        ++$index
        Start-Sleep -Seconds $IntervalSeconds
    }
}
finally {
    Pop-Location
}
