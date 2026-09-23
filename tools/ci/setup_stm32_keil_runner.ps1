param(
    [string]$Repo = "CS19970929/103-309-template",
    [string]$RunnerRoot = "C:\actions-runner-stm32-keil",
    [string]$RunnerName = "$env:COMPUTERNAME-stm32-keil",
    [string]$RegistrationToken = "",
    [bool]$InstallService = $true
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Find-FirstExisting([string[]]$Candidates) {
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }
    return $null
}

function Get-GhRunnerToken {
    param(
        [string]$RepoName,
        [ValidateSet("registration","remove")]
        [string]$Kind
    )

    $gh = Get-Command gh -ErrorAction SilentlyContinue
    if (-not $gh) {
        throw "GitHub CLI (gh) is required to repair/reconfigure an existing runner automatically. Install/authenticate gh, or remove the runner in GitHub Settings and rerun with -RegistrationToken."
    }

    $endpoint = if ($Kind -eq "registration") {
        "repos/$RepoName/actions/runners/registration-token"
    } else {
        "repos/$RepoName/actions/runners/remove-token"
    }

    $token = & gh api --method POST $endpoint --jq ".token"
    if ($LASTEXITCODE -ne 0 -or -not $token) {
        throw "Unable to obtain $Kind runner token through gh. Ensure gh is authenticated as the repository owner/admin."
    }
    return $token.Trim()
}

function Get-RegistrationToken {
    param([string]$RepoName, [string]$ExplicitToken)

    if ($ExplicitToken) {
        return $ExplicitToken
    }

    return Get-GhRunnerToken -RepoName $RepoName -Kind "registration"
}

function Find-RunnerService {
    param([string]$Root, [string]$Name)

    $services = Get-CimInstance Win32_Service -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -like "actions.runner.*" -and (
                ($_.PathName -and $_.PathName -like "*$Root*") -or
                ($_.DisplayName -and $_.DisplayName -like "*$Name*")
            )
        }

    return $services | Select-Object -First 1
}

$uv4 = Find-FirstExisting @(
    $env:KEIL_UV4,
    "C:\Keil_v5\UV4\UV4.exe",
    "C:\Keil\UV4\UV4.exe",
    "C:\Program Files\Keil_v5\UV4\UV4.exe",
    "C:\Program Files (x86)\Keil_v5\UV4\UV4.exe"
)
if (-not $uv4) {
    throw "Keil UV4.exe was not found. Install/repair Keil MDK first, or set KEIL_UV4."
}

$armcc = Find-FirstExisting @(
    $env:KEIL_ARMCC5,
    "C:\Keil_v5\ARM\ARMCC\bin\armcc.exe",
    "C:\Keil_v5\ARM\ARMCC_5.06u7\bin\armcc.exe",
    "C:\Keil\ARM\ARMCC\bin\armcc.exe"
)
if (-not $armcc) {
    Write-Warning "ARMCC5 armcc.exe was not found in common paths. The runner can be registered, but the production build may fail until ARM Compiler 5 is installed/configured."
}

Write-Host "Repository : $Repo"
Write-Host "Runner     : $RunnerName"
Write-Host "Runner root: $RunnerRoot"
Write-Host "Keil       : $uv4"
Write-Host "ARMCC5     : $armcc"

New-Item -ItemType Directory -Force -Path $RunnerRoot | Out-Null
Set-Location $RunnerRoot

if (-not (Test-Path ".\config.cmd")) {
    Write-Host "Downloading latest GitHub Actions runner..."
    $headers = @{ "User-Agent" = "stm32-keil-runner-setup" }
    $release = Invoke-RestMethod -Headers $headers -Uri "https://api.github.com/repos/actions/runner/releases/latest"
    $asset = $release.assets |
        Where-Object { $_.name -like "actions-runner-win-x64-*.zip" } |
        Select-Object -First 1
    if (-not $asset) {
        throw "Unable to find Windows x64 GitHub Actions runner asset."
    }

    $zip = Join-Path $RunnerRoot $asset.name
    Invoke-WebRequest -Headers $headers -Uri $asset.browser_download_url -OutFile $zip
    Expand-Archive -Path $zip -DestinationPath $RunnerRoot -Force
    Remove-Item -Force $zip
}

$service = Find-RunnerService -Root $RunnerRoot -Name $RunnerName

if ($InstallService -and (Test-Path ".\.runner") -and -not $service) {
    Write-Host "Existing runner is registered but not installed as a Windows service."
    Write-Host "Reconfiguring it with --runasservice..."

    $removeToken = Get-GhRunnerToken -RepoName $Repo -Kind "remove"
    & ".\config.cmd" remove --token $removeToken
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to remove the existing non-service runner configuration."
    }
}

if (-not (Test-Path ".\.runner")) {
    $token = Get-RegistrationToken -RepoName $Repo -ExplicitToken $RegistrationToken
    $args = @(
        "--unattended",
        "--url", "https://github.com/$Repo",
        "--token", $token,
        "--name", $RunnerName,
        "--labels", "stm32-keil,keil-armcc5",
        "--work", "_work",
        "--replace"
    )
    if ($InstallService) {
        $args += "--runasservice"
    }

    & ".\config.cmd" @args
    if ($LASTEXITCODE -ne 0) {
        throw "GitHub Actions runner registration failed."
    }
} else {
    Write-Host "Runner is already configured in this directory."
}

if ($InstallService) {
    Start-Sleep -Seconds 1
    $service = Find-RunnerService -Root $RunnerRoot -Name $RunnerName
    if (-not $service) {
        throw "Runner was configured with --runasservice, but its Windows service could not be found."
    }

    if ($service.State -ne "Running") {
        Start-Service -Name $service.Name
        Start-Sleep -Seconds 1
        $service = Get-CimInstance Win32_Service -Filter "Name='$($service.Name)'"
    }

    Write-Host "Service    : $($service.Name)"
    Write-Host "State      : $($service.State)"
    Write-Host "Start mode : $($service.StartMode)"
}

$gh = Get-Command gh -ErrorAction SilentlyContinue
if ($gh) {
    try {
        & gh variable set STM32_KEIL_CI_ENABLED --repo $Repo --body "1"
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Enabled repository variable STM32_KEIL_CI_ENABLED=1."
        }
    } catch {
        Write-Warning "Runner is configured, but the repository Actions variable could not be set automatically."
    }
}

Write-Host ""
Write-Host "PASS: STM32 Keil self-hosted runner setup completed."
Write-Host "Labels: self-hosted, Windows, X64, stm32-keil, keil-armcc5"
Write-Host "The Telink runner remains separate; this instance uses its own directory and Windows service."
