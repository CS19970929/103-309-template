param(
    [string]$Project = "103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx",
    [string]$Target = "",
    [switch]$FailOnWarnings
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Resolve-ExistingPath([string[]]$Candidates) {
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }
    return $null
}

function Find-FromElf([string]$Uv4Path) {
    if ($env:KEIL_FROMELF -and (Test-Path $env:KEIL_FROMELF)) {
        return (Resolve-Path $env:KEIL_FROMELF).Path
    }

    $uv4Dir = Split-Path -Parent $Uv4Path
    $keilRoot = Split-Path -Parent $uv4Dir
    $candidates = @(
        (Join-Path $keilRoot "ARM\ARMCC\bin\fromelf.exe"),
        (Join-Path $keilRoot "ARM\ARMCC_5.06u7\bin\fromelf.exe"),
        "C:\Keil_v5\ARM\ARMCC\bin\fromelf.exe",
        "C:\Keil_v5\ARM\ARMCC_5.06u7\bin\fromelf.exe",
        "C:\Keil\ARM\ARMCC\bin\fromelf.exe"
    )
    return Resolve-ExistingPath $candidates
}

$projectPath = (Resolve-Path $Project).Path
$repoRoot = (Get-Location).Path
$buildDir = Join-Path $repoRoot "build\keil"
$artifactDir = Join-Path $buildDir "artifacts"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
$logPath = Join-Path $buildDir "uv4-build.log"
$summaryPath = Join-Path $buildDir "build-summary.txt"

$uv4 = Resolve-ExistingPath @(
    $env:KEIL_UV4,
    "C:\Keil_v5\UV4\UV4.exe",
    "C:\Keil\UV4\UV4.exe",
    "C:\Program Files\Keil_v5\UV4\UV4.exe",
    "C:\Program Files (x86)\Keil_v5\UV4\UV4.exe"
)

if (-not $uv4) {
    throw "Keil UV4.exe not found. Set KEIL_UV4 or install Keil MDK on this runner."
}

[xml]$xml = Get-Content -LiteralPath $projectPath
$targetNode = $xml.Project.Targets.Target | Select-Object -First 1
if (-not $targetNode) {
    throw "No target found in $projectPath"
}

if (-not $Target) {
    $Target = [string]$targetNode.TargetName
}

$common = $targetNode.TargetOption.TargetCommonOption
$projectDir = Split-Path -Parent $projectPath
$outputDir = Join-Path $projectDir ([string]$common.OutputDirectory)
$listingDir = Join-Path $projectDir ([string]$common.ListingPath)
$outputName = [string]$common.OutputName

Write-Host "Keil     : $uv4"
Write-Host "Project  : $projectPath"
Write-Host "Target   : $Target"
Write-Host "Objects  : $outputDir"
Write-Host "Listings : $listingDir"
Write-Host "Log      : $logPath"

if (Test-Path $logPath) {
    Remove-Item -Force $logPath
}

$uvArgs = @(
    "-j0",
    "-b",
    ('"{0}"' -f $projectPath),
    "-t",
    ('"{0}"' -f $Target),
    "-o",
    ('"{0}"' -f $logPath)
)

$process = Start-Process -FilePath $uv4 -ArgumentList $uvArgs -Wait -PassThru -NoNewWindow
$exitCode = $process.ExitCode

if (Test-Path $logPath) {
    Get-Content -LiteralPath $logPath
}

if (-not (Test-Path $logPath)) {
    throw "Keil did not produce a build log."
}

$logText = Get-Content -Raw -LiteralPath $logPath
$errorMatch = [regex]::Match($logText, '(\d+)\s+Error\(s\)')
$warningMatch = [regex]::Match($logText, '(\d+)\s+Warning\(s\)')
if (-not $errorMatch.Success) {
    throw "Unable to find Keil error summary in build log."
}

$errorCount = [int]$errorMatch.Groups[1].Value
$warningCount = if ($warningMatch.Success) { [int]$warningMatch.Groups[1].Value } else { -1 }

if ($errorCount -ne 0) {
    throw "Keil production build failed with $errorCount error(s). UV4 exit code: $exitCode"
}
if ($exitCode -ne 0) {
    Write-Warning "UV4 returned exit code $exitCode, but the Keil build log reports 0 errors. Treating the production build as successful."
}
if ($FailOnWarnings -and $warningCount -gt 0) {
    throw "Keil production build has $warningCount warning(s) and FailOnWarnings is enabled."
}

$copyPatterns = @("*.axf", "*.hex", "*.bin", "*.map", "*.lst", "*.lnp", "*.htm", "*.html")
foreach ($dir in @($outputDir, $listingDir)) {
    if (-not (Test-Path $dir)) {
        continue
    }
    foreach ($pattern in $copyPatterns) {
        Get-ChildItem -Path $dir -Filter $pattern -File -ErrorAction SilentlyContinue |
            ForEach-Object {
                Copy-Item -Force $_.FullName (Join-Path $artifactDir $_.Name)
            }
    }
}

$axf = Get-ChildItem -Path $artifactDir -Filter "*.axf" -File -ErrorAction SilentlyContinue |
       Select-Object -First 1
$fromelf = Find-FromElf $uv4
if ($axf -and $fromelf) {
    $binPath = Join-Path $artifactDir ($outputName + ".bin")
    $hexPath = Join-Path $artifactDir ($outputName + ".hex")

    & $fromelf --bin --output $binPath $axf.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "fromelf failed to generate BIN."
    }

    & $fromelf --i32combined --output $hexPath $axf.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "fromelf failed to generate HEX."
    }
    Write-Host "fromelf  : $fromelf"
}

$programSize = [regex]::Match($logText, 'Program Size:[^\r\n]*')
$map = Get-ChildItem -Path $artifactDir -Filter "*.map" -File -ErrorAction SilentlyContinue |
       Select-Object -First 1

if ($map) {
    Write-Host ""
    Write-Host "=== ARMCC MAP SUMMARY ==="
    $mapText = Get-Content -Raw -LiteralPath $map.FullName

    $roTotal = [regex]::Match($mapText, 'Total RO\s+Size[^\r\n]*')
    $rwTotal = [regex]::Match($mapText, 'Total RW\s+Size[^\r\n]*')
    $romTotal = [regex]::Match($mapText, 'Total ROM Size[^\r\n]*')

    foreach ($m in @($roTotal, $rwTotal, $romTotal)) {
        if ($m.Success) {
            Write-Host $m.Value.Trim()
        }
    }

    $objectRows = @()
    foreach ($line in (Get-Content -LiteralPath $map.FullName)) {
        if ($line -match '^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(.+\.o)\s*
    "target=$Target",
    "uv4=$uv4",
    "fromelf=$fromelf",
    "uv4_exit_code=$exitCode",
    "errors=$errorCount",
    "warnings=$warningCount",
    "program_size=$($programSize.Value)"
)
$summary | Set-Content -Encoding UTF8 -LiteralPath $summaryPath

Write-Host ""
Write-Host "PASS: Keil ARMCC5 production build completed."
Write-Host "Errors   : $errorCount"
Write-Host "Warnings : $warningCount"
if ($programSize.Success) {
    Write-Host $programSize.Value
}
Write-Host "Artifacts: $artifactDir"
) {
            $objectRows += [pscustomobject]@{
                Code = [int]$matches[1]
                IncData = [int]$matches[2]
                RO = [int]$matches[3]
                RW = [int]$matches[4]
                ZI = [int]$matches[5]
                Debug = [int]$matches[6]
                Name = $matches[7].Trim()
                ROM = ([int]$matches[1] + [int]$matches[3] + [int]$matches[4])
                RAM = ([int]$matches[4] + [int]$matches[5])
            }
        }
    }

    if ($objectRows.Count -gt 0) {
        Write-Host ""
        Write-Host "Top objects by ROM:"
        $objectRows | Sort-Object ROM -Descending | Select-Object -First 12 |
            Format-Table Name,Code,RO,RW,ROM -AutoSize | Out-String | Write-Host

        Write-Host "Top objects by RAM:"
        $objectRows | Sort-Object RAM -Descending | Select-Object -First 12 |
            Format-Table Name,RW,ZI,RAM -AutoSize | Out-String | Write-Host
    }
} else {
    Write-Warning "No ARMCC map file was found in the Keil Listings/Objects directories."
}

$summary = @(
    "project=$projectPath",
    "target=$Target",
    "uv4=$uv4",
    "fromelf=$fromelf",
    "errors=$errorCount",
    "warnings=$warningCount",
    "program_size=$($programSize.Value)"
)
$summary | Set-Content -Encoding UTF8 -LiteralPath $summaryPath

Write-Host ""
Write-Host "PASS: Keil ARMCC5 production build completed."
Write-Host "Errors   : $errorCount"
Write-Host "Warnings : $warningCount"
if ($programSize.Success) {
    Write-Host $programSize.Value
}
Write-Host "Artifacts: $artifactDir"
