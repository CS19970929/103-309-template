param(
    [string]$Port = "",
    [int]$Baud = 115200,
    [string]$Bin = "",
    [string]$PythonVersion = "3.9"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Py = (Get-Command py.exe -ErrorAction Stop).Source

function Quote-ProcessArg([string]$Value) {
    if ($Value -match '[\s"]') {
        return '"' + ($Value -replace '"', '\"') + '"'
    }
    return $Value
}

if ([string]::IsNullOrWhiteSpace($Bin)) {
    $Bin = Join-Path $RepoRoot "103 + 309\Project\Users\Objects\FD_Release.bin"
}

Push-Location $RepoRoot
try {
    & $Py "-$PythonVersion" "tools\comm_tool_upgrade_ui.py" --self-test
    if ($LASTEXITCODE -ne 0) {
        throw "Python dependency check failed"
    }

    $ArgsList = @(
        "-$PythonVersion",
        "tools\comm_tool_upgrade_ui.py",
        "--baud", [string]$Baud,
        "--bin", $Bin
    )
    if (-not [string]::IsNullOrWhiteSpace($Port)) {
        $ArgsList += @("--port", $Port)
    }

    $ArgString = ($ArgsList | ForEach-Object { Quote-ProcessArg $_ }) -join " "
    $Process = Start-Process -FilePath $Py -ArgumentList $ArgString -WorkingDirectory $RepoRoot -PassThru
    Write-Host ("comm tool upgrade UI started. PID={0}" -f $Process.Id)
}
catch {
    Write-Error @"
comm tool 升级上位机启动失败。

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
