param(
    [string]$Target = "COMM_TOOL_Release",
    [string]$Project = "firmware\comm_tool_f103ret6\keil\COMM_TOOL_F103RET6.uvprojx",
    [string]$UV4Path = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$projectPath = Join-Path $repoRoot $Project

if (-not (Test-Path -LiteralPath $projectPath)) {
    throw "Keil project not found: $projectPath"
}

if ([string]::IsNullOrWhiteSpace($UV4Path)) {
    $candidates = @()
    if ($env:KEIL_ROOT) {
        $candidates += (Join-Path $env:KEIL_ROOT "UV4\UV4.exe")
    }
    if ($env:MDK_ROOT) {
        $candidates += (Join-Path $env:MDK_ROOT "UV4\UV4.exe")
    }
    $candidates += "C:\Keil_v5\UV4\UV4.exe"
    $candidates += "C:\Keil\UV4\UV4.exe"
    $candidates += "${env:ProgramFiles(x86)}\Keil_v5\UV4\UV4.exe"
    $candidates += "${env:ProgramFiles}\Keil_v5\UV4\UV4.exe"

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            $UV4Path = $candidate
            break
        }
    }
}

if ([string]::IsNullOrWhiteSpace($UV4Path) -or -not (Test-Path -LiteralPath $UV4Path)) {
    throw "UV4.exe not found. Pass -UV4Path or set KEIL_ROOT/MDK_ROOT."
}

[xml]$projectXml = Get-Content -LiteralPath $projectPath
$targetNames = @($projectXml.Project.Targets.Target | ForEach-Object { $_.TargetName })
if ($targetNames -notcontains $Target) {
    throw "Target '$Target' not found. Available targets: $($targetNames -join ', ')"
}

$logDir = Join-Path (Split-Path -Parent $projectPath) "build_logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$logPath = Join-Path $logDir "$Target.log"

Write-Host "Project: $projectPath"
Write-Host "Target : $Target"
Write-Host "UV4    : $UV4Path"
Write-Host "Log    : $logPath"

& $UV4Path -b $projectPath -t $Target -j0 -o $logPath
$exitCode = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
$logText = ""
if (Test-Path -LiteralPath $logPath) {
    $logText = Get-Content -LiteralPath $logPath -Raw
}

if (($exitCode -ne 0) -or ($logText -match '[1-9][0-9]* Error\(s\)')) {
    throw "Keil build failed with exit code $exitCode. See $logPath"
}

Write-Host "Keil build finished."
