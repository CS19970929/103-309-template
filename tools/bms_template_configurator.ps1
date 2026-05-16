param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Args
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PythonScript = Join-Path $ScriptDir "bms_template_configurator.py"

if (Get-Command py -ErrorAction SilentlyContinue) {
    & py -3 $PythonScript @Args
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
    & python $PythonScript @Args
} else {
    throw "Python not found. Install Python or use the Windows Python Launcher 'py'."
}
