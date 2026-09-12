param(
    [string]$Project = "C030v1.0/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx"
)

$ErrorActionPreference = "Stop"

$projectPath = (Resolve-Path $Project).Path
$buildDir = Join-Path (Get-Location) "build/keil"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$logPath = Join-Path $buildDir "uv4-build.log"

$uv4 = $env:KEIL_UV4
if (-not $uv4) {
    $candidates = @(
        "C:\Keil_v5\UV4\UV4.exe",
        "C:\Keil\UV4\UV4.exe"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            $uv4 = $candidate
            break
        }
    }
}

if (-not $uv4 -or -not (Test-Path $uv4)) {
    throw "Keil UV4.exe not found. Set KEIL_UV4 or install Keil MDK on the runner."
}

Write-Host "Keil:    $uv4"
Write-Host "Project: $projectPath"
Write-Host "Log:     $logPath"

& $uv4 -b $projectPath -j0 -o $logPath
$exitCode = $LASTEXITCODE

if (Test-Path $logPath) {
    Get-Content $logPath
}

if ($exitCode -ne 0) {
    throw "Keil build process returned exit code $exitCode"
}

if (-not (Test-Path $logPath)) {
    throw "Keil did not produce a build log."
}

$success = Select-String -Path $logPath -Pattern "0 Error\(s\)" -Quiet
if (-not $success) {
    throw "Keil build log does not contain '0 Error(s)'."
}

Write-Host "PASS: Keil ARMCC5 full firmware build completed with 0 errors."
