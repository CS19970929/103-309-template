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
    return Resolve-ExistingPath @(
        (Join-Path $keilRoot "ARM\ARMCC\bin\fromelf.exe"),
        (Join-Path $keilRoot "ARM\ARMCC_5.06u7\bin\fromelf.exe"),
        "C:\Keil_v5\ARM\ARMCC\bin\fromelf.exe",
        "C:\Keil_v5\ARM\ARMCC_5.06u7\bin\fromelf.exe",
        "C:\Keil\ARM\ARMCC\bin\fromelf.exe"
    )
}

function Invoke-NativeProcess {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $process = Start-Process -FilePath $FilePath -ArgumentList $Arguments -Wait -PassThru -NoNewWindow
    return $process.ExitCode
}

$projectPath = (Resolve-Path $Project).Path
$repoRoot = (Get-Location).Path
$buildDir = Join-Path $repoRoot "build\keil"
$artifactDir = Join-Path $buildDir "artifacts"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null

$logPath = Join-Path $buildDir "uv4-build.log"
$summaryPath = Join-Path $buildDir "build-summary.txt"
$mapSummaryPath = Join-Path $buildDir "map-summary.txt"

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

$uvExitCode = Invoke-NativeProcess -FilePath $uv4 -Arguments $uvArgs

if (-not (Test-Path $logPath)) {
    throw "Keil did not produce a build log. UV4 exit code: $uvExitCode"
}

Get-Content -LiteralPath $logPath
$logText = Get-Content -Raw -LiteralPath $logPath

$errorMatch = [regex]::Match($logText, '(\d+)\s+Error\(s\)')
$warningMatch = [regex]::Match($logText, '(\d+)\s+Warning\(s\)')
if (-not $errorMatch.Success) {
    throw "Unable to find Keil error summary in build log. UV4 exit code: $uvExitCode"
}

$errorCount = [int]$errorMatch.Groups[1].Value
$warningCount = if ($warningMatch.Success) { [int]$warningMatch.Groups[1].Value } else { -1 }

if ($errorCount -ne 0) {
    throw "Keil production build failed with $errorCount error(s). UV4 exit code: $uvExitCode"
}

if ($uvExitCode -ne 0) {
    Write-Warning "UV4 returned exit code $uvExitCode, but the Keil build log reports 0 errors. Build accepted."
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

    $binExit = Invoke-NativeProcess -FilePath $fromelf -Arguments @(
        "--bin",
        "--output", ('"{0}"' -f $binPath),
        ('"{0}"' -f $axf.FullName)
    )
    if ($binExit -ne 0) {
        throw "fromelf failed to generate BIN, exit code $binExit."
    }

    $hexExit = Invoke-NativeProcess -FilePath $fromelf -Arguments @(
        "--i32combined",
        "--output", ('"{0}"' -f $hexPath),
        ('"{0}"' -f $axf.FullName)
    )
    if ($hexExit -ne 0) {
        throw "fromelf failed to generate HEX, exit code $hexExit."
    }

    Write-Host "fromelf  : $fromelf"
}

$programSize = [regex]::Match($logText, 'Program Size:\s*Code=(\d+)\s+RO-data=(\d+)\s+RW-data=(\d+)\s+ZI-data=(\d+)')
$flashUsed = $null
$ramUsed = $null
$flashCapacity = $null
$ramCapacity = $null

if ($programSize.Success) {
    $codeBytes = [int]$programSize.Groups[1].Value
    $roBytes = [int]$programSize.Groups[2].Value
    $rwBytes = [int]$programSize.Groups[3].Value
    $ziBytes = [int]$programSize.Groups[4].Value

    $flashUsed = $codeBytes + $roBytes + $rwBytes
    $ramUsed = $rwBytes + $ziBytes

    $cpuText = [string]$common.Cpu
    $iromMatch = [regex]::Match($cpuText, 'IROM\(0x[0-9A-Fa-f]+,0x([0-9A-Fa-f]+)\)')
    $iramMatch = [regex]::Match($cpuText, 'IRAM\(0x[0-9A-Fa-f]+,0x([0-9A-Fa-f]+)\)')

    if ($iromMatch.Success) {
        $flashCapacity = [Convert]::ToInt32($iromMatch.Groups[1].Value, 16)
    }
    if ($iramMatch.Success) {
        $ramCapacity = [Convert]::ToInt32($iramMatch.Groups[1].Value, 16)
    }
}

$map = Get-ChildItem -Path $artifactDir -Filter "*.map" -File -ErrorAction SilentlyContinue |
       Select-Object -First 1

$mapSummary = New-Object System.Collections.Generic.List[string]
$mapSummary.Add("=== ARMCC MAP SUMMARY ===")

if ($programSize.Success) {
    $mapSummary.Add("Program Size: Code=$codeBytes RO-data=$roBytes RW-data=$rwBytes ZI-data=$ziBytes")
    $mapSummary.Add("Flash used: $flashUsed bytes")
    $mapSummary.Add("RAM used: $ramUsed bytes")

    if ($flashCapacity) {
        $flashRemain = $flashCapacity - $flashUsed
        $flashPct = [Math]::Round(($flashUsed * 100.0) / $flashCapacity, 2)
        $mapSummary.Add("Flash capacity: $flashCapacity bytes, remaining: $flashRemain bytes, used: $flashPct%")
    }

    if ($ramCapacity) {
        $ramRemain = $ramCapacity - $ramUsed
        $ramPct = [Math]::Round(($ramUsed * 100.0) / $ramCapacity, 2)
        $mapSummary.Add("RAM capacity: $ramCapacity bytes, remaining: $ramRemain bytes, used: $ramPct%")
    }
}

if ($map) {
    $mapSummary.Add("Map file: $($map.Name)")
    $mapText = Get-Content -Raw -LiteralPath $map.FullName

    foreach ($pattern in @(
        "Total RO\s+Size[^\r\n]*",
        "Total RW\s+Size[^\r\n]*",
        "Total ROM Size[^\r\n]*"
    )) {
        $match = [regex]::Match($mapText, $pattern)
        if ($match.Success) {
            $mapSummary.Add($match.Value.Trim())
        }
    }

    $objectRows = @()
    foreach ($line in (Get-Content -LiteralPath $map.FullName)) {
        if ($line -match "^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(.+\.o)\s*$") {
            $code = [int]$matches[1]
            $incData = [int]$matches[2]
            $ro = [int]$matches[3]
            $rw = [int]$matches[4]
            $zi = [int]$matches[5]
            $debug = [int]$matches[6]
            $name = $matches[7].Trim()

            $objectRows += [pscustomobject]@{
                Name = $name
                Code = $code
                IncData = $incData
                RO = $ro
                RW = $rw
                ZI = $zi
                Debug = $debug
                ROM = $code + $ro + $rw
                RAM = $rw + $zi
            }
        }
    }

    if ($objectRows.Count -gt 0) {
        $mapSummary.Add("")
        $mapSummary.Add("Top objects by ROM:")
        foreach ($row in ($objectRows | Sort-Object ROM -Descending | Select-Object -First 12)) {
            $mapSummary.Add(("{0,-32} Code={1,6} RO={2,6} RW={3,6} ROM={4,6}" -f $row.Name, $row.Code, $row.RO, $row.RW, $row.ROM))
        }

        $mapSummary.Add("")
        $mapSummary.Add("Top objects by RAM:")
        foreach ($row in ($objectRows | Sort-Object RAM -Descending | Select-Object -First 12)) {
            $mapSummary.Add(("{0,-32} RW={1,6} ZI={2,6} RAM={3,6}" -f $row.Name, $row.RW, $row.ZI, $row.RAM))
        }
    } else {
        $mapSummary.Add("No object-size rows matched the expected ARMCC5 map format.")
    }
} else {
    $mapSummary.Add("No ARMCC map file was found in Listings/Objects.")
}

$mapSummary | Set-Content -Encoding UTF8 -LiteralPath $mapSummaryPath
Write-Host ""
$mapSummary | ForEach-Object { Write-Host $_ }

$summary = @(
    "project=$projectPath",
    "target=$Target",
    "uv4=$uv4",
    "fromelf=$fromelf",
    "uv4_exit_code=$uvExitCode",
    "errors=$errorCount",
    "warnings=$warningCount",
    "flash_used_bytes=$flashUsed",
    "ram_used_bytes=$ramUsed",
    "flash_capacity_bytes=$flashCapacity",
    "ram_capacity_bytes=$ramCapacity"
)
$summary | Set-Content -Encoding UTF8 -LiteralPath $summaryPath

Write-Host ""
Write-Host "PASS: Keil ARMCC5 production build completed."
Write-Host "Errors   : $errorCount"
Write-Host "Warnings : $warningCount"
if ($programSize.Success) {
    Write-Host "Program Size: Code=$codeBytes RO-data=$roBytes RW-data=$rwBytes ZI-data=$ziBytes"
}
Write-Host "Artifacts: $artifactDir"
