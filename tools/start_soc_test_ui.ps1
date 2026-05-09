param(
    [string]$Port = "COM4",
    [int]$Baud = 19200,
    [int]$Slave = 1,
    [int]$Samples = 60,
    [double]$Interval = 1.0,
    [string]$PythonVersion = "3.9",
    [switch]$Demo
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Py = (Get-Command py.exe -ErrorAction Stop).Source

Push-Location $RepoRoot
try {
    & $Py "-$PythonVersion" -c "import serial; import tkinter; print('Python OK:', __import__('sys').executable)"
    if ($LASTEXITCODE -ne 0) {
        throw "Python dependency check failed"
    }

    $UiArgs = @(
        "-$PythonVersion",
        "tools\soc_test_ui.py",
        "--port", $Port,
        "--baud", [string]$Baud,
        "--slave", [string]$Slave,
        "--samples", [string]$Samples,
        "--interval", [string]$Interval
    )
    if ($Demo) {
        $UiArgs += "--demo"
    }

    $Process = Start-Process -FilePath $Py -ArgumentList $UiArgs -WorkingDirectory $RepoRoot -PassThru
    Write-Host ("SOC test UI started. PID={0}" -f $Process.Id)
}
catch {
    Write-Error @"
SOC 测试上位机启动失败。

请确认使用 Windows Python Launcher 的 py 环境，并安装 pyserial：
  py -$PythonVersion -m pip install pyserial

可用解释器列表：
  py -0p

错误信息：
$($_.Exception.Message)
"@
    exit 1
}
finally {
    Pop-Location
}
