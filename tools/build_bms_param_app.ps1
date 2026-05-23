param(
    [string]$Project = "103 + 309\Project\Users\CommomSH367309_16series_103RCT6_C.uvprojx",
    [string]$BaseTarget = "FD_Release",
    [string]$ParamTarget = "FD_Param",
    [string]$Uv4Path = "C:\Keil_v5\UV4\UV4.exe"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$projectPath = (Resolve-Path (Join-Path $repoRoot $Project)).Path
$projectDir = Split-Path -Parent $projectPath
$tempProject = Join-Path $projectDir "codex_param_build.uvprojx"
$logPath = Join-Path $projectDir "codex_param_build.log"
$binPath = Join-Path $projectDir "Objects\$ParamTarget.bin"
$configPath = (Resolve-Path (Join-Path $repoRoot "103 + 309\Project\Source\conf\Project_Config.h")).Path
$configOriginalBytes = $null
$configOriginalLastWriteTimeUtc = $null

if (!(Test-Path -LiteralPath $Uv4Path)) {
    throw "Keil UV4.exe not found: $Uv4Path"
}

try {
    Write-Host "Preparing BMS parameter build..."
    $configEncoding = [System.Text.Encoding]::Default
    $configOriginalBytes = [System.IO.File]::ReadAllBytes($configPath)
    $configOriginalLastWriteTimeUtc = (Get-Item -LiteralPath $configPath).LastWriteTimeUtc
    $configText = $configEncoding.GetString($configOriginalBytes)
    $configPattern = "(?m)^#define\s+PROJECT_CFG_HOST_WRITE_ENABLE\s+[01]\s*$"
    if ($configText -notmatch $configPattern) {
        throw "PROJECT_CFG_HOST_WRITE_ENABLE define not found in Project_Config.h"
    }
    $configRegex = [System.Text.RegularExpressions.Regex]::new($configPattern)
    $configText = $configRegex.Replace($configText, "#define PROJECT_CFG_HOST_WRITE_ENABLE 1", 1)
    [System.IO.File]::WriteAllText($configPath, $configText, $configEncoding)

    [xml]$xml = Get-Content -LiteralPath $projectPath -Raw -Encoding UTF8
    $target = $xml.SelectSingleNode("//Target[TargetName='$BaseTarget']")
    if ($null -eq $target) {
        throw "Keil target not found: $BaseTarget"
    }

    $targetNameNode = $target.SelectSingleNode("TargetName")
    if ($null -eq $targetNameNode) {
        throw "TargetName node not found"
    }
    $targetNameNode.InnerText = $ParamTarget
    $outputNode = $target.SelectSingleNode(".//TargetCommonOption/OutputName")
    if ($null -ne $outputNode) {
        $outputNode.InnerText = $ParamTarget
    }

    $settings = New-Object System.Xml.XmlWriterSettings
    $settings.Encoding = New-Object System.Text.UTF8Encoding($false)
    $settings.Indent = $true
    $writer = [System.Xml.XmlWriter]::Create($tempProject, $settings)
    $xml.Save($writer)
    $writer.Close()

    if (Test-Path -LiteralPath $binPath) {
        Remove-Item -LiteralPath $binPath -Force
    }

    Write-Host "Running Keil target $ParamTarget..."
    $argLine = "-r `"$tempProject`" -t `"$ParamTarget`" -j0 -o `"$logPath`""
    $process = Start-Process -FilePath $Uv4Path -ArgumentList $argLine -Wait -PassThru -WindowStyle Hidden
    $keilExitCode = $process.ExitCode

    if (!(Test-Path -LiteralPath $binPath)) {
        throw "Keil build failed and bin was not found. Keil exit code: $keilExitCode"
    }
    if ($keilExitCode -ne 0) {
        Write-Warning "Keil returned exit code $keilExitCode, but bin exists. Check Objects\\$ParamTarget.build_log.htm for warnings."
    }
    Write-Host "BMS parameter firmware generated:"
    Write-Host $binPath
}
finally {
    if (Test-Path -LiteralPath $tempProject) {
        Remove-Item -LiteralPath $tempProject -Force
    }
    if ($null -ne $configOriginalBytes) {
        [System.IO.File]::WriteAllBytes($configPath, $configOriginalBytes)
        if ($null -ne $configOriginalLastWriteTimeUtc) {
            (Get-Item -LiteralPath $configPath).LastWriteTimeUtc = $configOriginalLastWriteTimeUtc
        }
    }
}
