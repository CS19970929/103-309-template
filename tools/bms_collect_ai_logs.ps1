param(
    [string]$LogDir = "logs\bms_watch",
    [string]$Out = "",
    [int]$TailLines = 220
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$ResolvedLogDir = Join-Path $RepoRoot $LogDir
New-Item -ItemType Directory -Force -Path $ResolvedLogDir | Out-Null

if ([string]::IsNullOrWhiteSpace($Out)) {
    $Out = Join-Path $ResolvedLogDir ("ai_context_{0}.md" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
}
elseif (-not [IO.Path]::IsPathRooted($Out)) {
    $Out = Join-Path $RepoRoot $Out
}

function Add-Section {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Title,
        [string[]]$Body
    )
    $Lines.Add("")
    $Lines.Add("## $Title")
    $Lines.Add("")
    foreach ($line in $Body) {
        $Lines.Add($line)
    }
}

function Run-Text {
    param([scriptblock]$Action)
    try {
        return (& $Action 2>&1 | Out-String).TrimEnd().Split("`n")
    }
    catch {
        return @("ERROR: $($_.Exception.Message)")
    }
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# BMS AI Context Log")
$lines.Add("")
$lines.Add("- generated_at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
$lines.Add("- repo: $RepoRoot")
$lines.Add("- purpose: compact local test/debug logs for optional AI review.")

Push-Location $RepoRoot
try {
    Add-Section $lines "Git Status" (Run-Text { git status --short })
    Add-Section $lines "Recent Commits" (Run-Text { git log --oneline -5 })
    Add-Section $lines "Key Config" (Run-Text {
        Select-String -Path "103 + 309\Project\Source\conf\Project_Config.h" `
            -Pattern "PROJECT_CFG_BUILD_PROFILE|PROJECT_CFG_WDOG_ENABLE|PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA|PROJECT_CFG_SOC_TEST_MODE_ENABLE|PROJECT_CFG_DEBUG_WATCH_ENABLE" |
            ForEach-Object { "{0}:{1}: {2}" -f $_.Path,$_.LineNumber,$_.Line.Trim() }
    })
    Add-Section $lines "Project Check" (Run-Text { py -3.9 tools\project_check.py -q })

    $latestLogs = Get-ChildItem -LiteralPath $ResolvedLogDir -Filter "*.log" -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 5
    if ($latestLogs) {
        foreach ($log in $latestLogs) {
            Add-Section $lines ("Log Tail: {0}" -f $log.Name) @('```text')
            Get-Content -LiteralPath $log.FullName -Tail $TailLines | ForEach-Object { $lines.Add($_) }
            $lines.Add('```')
        }
    }
    else {
        Add-Section $lines "Log Tail" @("No local monitor logs found under logs\bms_watch.")
    }
}
finally {
    Pop-Location
}

$parent = Split-Path -Parent $Out
if ($parent) {
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
}
$lines | Set-Content -LiteralPath $Out -Encoding UTF8
Write-Host "AI context log written:"
Write-Host "  $Out"
