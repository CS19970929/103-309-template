param(
    [string]$Target = "FD_Release",
    [string]$Uv4 = "",
    [switch]$Build
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Py = (Get-Command py.exe -ErrorAction SilentlyContinue)

if ($null -eq $Py) {
    throw "Windows Python Launcher py.exe not found. Install Python or run tools\check_rewrite_keil_project.py manually."
}

Push-Location $RepoRoot
try {
    $ArgsList = @(
        "-3",
        "tools\check_rewrite_keil_project.py",
        "--target", $Target
    )

    if ($Uv4 -ne "") {
        $ArgsList += @("--uv4", $Uv4)
    }
    if ($Build) {
        $ArgsList += "--build"
    }

    & $Py.Source @ArgsList
    if ($LASTEXITCODE -ne 0) {
        throw "Rewrite Keil validation/build failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}
