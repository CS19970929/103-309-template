param(
    [string]$PythonVersion = "3.9",
    [switch]$Clean,
    [switch]$Run
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Py = (Get-Command py.exe -ErrorAction Stop).Source
$AppName = "BMS_CAN_Host_UI"

function Ensure-PythonModule {
    param(
        [string]$ImportName,
        [string]$PackageName
    )

    & $Py "-$PythonVersion" -c "import $ImportName"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Installing $PackageName for Python $PythonVersion..."
        & $Py "-$PythonVersion" -m pip install $PackageName
        if ($LASTEXITCODE -ne 0) {
            throw "$PackageName install failed"
        }
    }
}

Push-Location $RepoRoot
try {
    & $Py "-$PythonVersion" -c "import tkinter; print('Python OK:', __import__('sys').executable)"
    if ($LASTEXITCODE -ne 0) {
        throw "Python tkinter dependency check failed"
    }

    Ensure-PythonModule -ImportName "can" -PackageName "python-can"
    Ensure-PythonModule -ImportName "PyInstaller" -PackageName "pyinstaller"

    $Running = Get-Process -Name $AppName -ErrorAction SilentlyContinue
    if ($Running) {
        throw "请先关闭正在运行的 $AppName.exe 后再打包，否则 Windows 会锁定 dist\$AppName.exe"
    }

    if ($Clean) {
        foreach ($Path in @("build\$AppName", "dist\$AppName.exe", "$AppName.spec")) {
            if (Test-Path -LiteralPath $Path) {
                Remove-Item -LiteralPath $Path -Recurse -Force
            }
        }
    }

    & $Py "-$PythonVersion" -m PyInstaller `
        --noconfirm `
        --clean `
        --windowed `
        --onefile `
        --name $AppName `
        --paths "tools" `
        --hidden-import can `
        --hidden-import can.interfaces.pcan `
        --hidden-import can.interfaces.kvaser `
        --hidden-import can.interfaces.slcan `
        --hidden-import can.interfaces.virtual `
        --collect-submodules can.interfaces `
        "tools\can_bms_host_ui.py"
    if ($LASTEXITCODE -ne 0) {
        throw "PyInstaller build failed"
    }

    $ExePath = Join-Path $RepoRoot "dist\$AppName.exe"
    if (!(Test-Path -LiteralPath $ExePath)) {
        throw "EXE not found: $ExePath"
    }
    Write-Host "CAN BMS host UI EXE built:"
    Write-Host $ExePath

    if ($Run) {
        Start-Process -FilePath $ExePath -WorkingDirectory (Split-Path -Parent $ExePath)
    }
}
finally {
    Pop-Location
}
