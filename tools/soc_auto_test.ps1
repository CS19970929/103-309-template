param(
    [string]$Port = "",
    [int]$Baud = 19200,
    [int]$Slave = 1,
    [int]$OnlineSamples = 20,
    [double]$OnlineInterval = 1.0,
    [string]$Report = "SOC_RIDE_SIM_REPORT.md",
    [string]$Csv = "SOC_RIDE_SIM_SAMPLES.csv",
    [string]$OnlineCsv = "SOC_ONLINE_MONITOR.csv"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

Write-Host "[1/3] SOC host replay tests"
py "tools\soc_replay_test.py"

Write-Host "[2/3] Accelerated ride simulation report"
py "tools\soc_ride_sim_report.py" --report $Report --csv $Csv

if ($Port -ne "") {
    Write-Host "[3/3] Online board monitor on $Port@$Baud"
    py "tools\soc_online_monitor.py" --port $Port --baud $Baud --slave $Slave --samples $OnlineSamples --interval $OnlineInterval --csv $OnlineCsv
} else {
    Write-Host "[3/3] Online board monitor skipped: pass -Port COMx to enable"
}
