param(
    [string]$PythonVersion = "3.9"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Py = (Get-Command py.exe -ErrorAction Stop).Source

Push-Location $RepoRoot
try {
    & $Py "-$PythonVersion" "tools\bms_dev_dashboard.py"
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
