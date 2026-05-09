param(
    [string]$PythonVersion = "3.9",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Py = (Get-Command py.exe -ErrorAction Stop).Source
$AppName = "BMS_SOC_Test_UI"

Push-Location $RepoRoot
try {
    & $Py "-$PythonVersion" -c "import serial; import tkinter; import PyInstaller; print('Build Python OK:', __import__('sys').executable)"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Installing PyInstaller for Python $PythonVersion..."
        & $Py "-$PythonVersion" -m pip install pyinstaller
        if ($LASTEXITCODE -ne 0) {
            throw "PyInstaller install failed"
        }
    }

    if ($Clean) {
        foreach ($Path in @("build\$AppName", "dist\$AppName.exe", "$AppName.spec")) {
            if (Test-Path -LiteralPath $Path) {
                Remove-Item -LiteralPath $Path -Recurse -Force
            }
        }
    }

    $Running = Get-Process -Name $AppName -ErrorAction SilentlyContinue
    if ($Running) {
        throw "请先关闭正在运行的 $AppName.exe 后再打包，否则 Windows 会锁定 dist\$AppName.exe"
    }

    & $Py "-$PythonVersion" -m PyInstaller `
        --noconfirm `
        --clean `
        --windowed `
        --onefile `
        --name $AppName `
        --paths "tools" `
        --hidden-import serial `
        --hidden-import serial.tools.list_ports `
        "tools\soc_test_ui.py"
    if ($LASTEXITCODE -ne 0) {
        throw "PyInstaller build failed"
    }

    $ExePath = Join-Path $RepoRoot "dist\$AppName.exe"
    if (!(Test-Path -LiteralPath $ExePath)) {
        throw "EXE not found: $ExePath"
    }
    Write-Host "SOC test UI EXE built:"
    Write-Host $ExePath
}
finally {
    Pop-Location
}
