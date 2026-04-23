param(
    [string]$MapFile,
    [string]$FlashHeader
)

$ErrorActionPreference = "Stop"

function Get-HexMacroValue {
    param(
        [string]$HeaderContent,
        [string]$MacroName
    )

    $match = [regex]::Match(
        $HeaderContent,
        "(?m)^\s*#define\s+$([regex]::Escape($MacroName))\b[^\r\n]*?(0x[0-9A-Fa-f]+)"
    )

    if (-not $match.Success) {
        throw "Cannot find hex value for macro $MacroName in $FlashHeader."
    }

    return [Convert]::ToUInt32($match.Groups[1].Value, 16)
}

if (-not (Test-Path -LiteralPath $MapFile)) {
    throw "Map file not found: $MapFile"
}

if (-not (Test-Path -LiteralPath $FlashHeader)) {
    throw "Flash header not found: $FlashHeader"
}

$headerContent = Get-Content -LiteralPath $FlashHeader -Raw
$appStart = Get-HexMacroValue -HeaderContent $headerContent -MacroName "FLASH_ADDR_APP_START"

$reservedNames = @(
    "FLASH_ADDR_STORAGE_AFE_SLOT_A",
    "FLASH_ADDR_STORAGE_AFE_SLOT_B",
    "FLASH_ADDR_STORAGE_LOG_SLOT_A",
    "FLASH_ADDR_STORAGE_LOG_SLOT_B",
    "FLASH_ADDR_SH367309_VALUE",
    "FLASH_ADDR_SH367309_FLAG",
    "FLASH_ADDR_UPDATE_FLAG",
    "FLASH_ADDR_SLEEP_FLAG"
)

$reservedPages = foreach ($name in $reservedNames) {
    [PSCustomObject]@{
        Name    = $name
        Address = Get-HexMacroValue -HeaderContent $headerContent -MacroName $name
    }
}

$firstReserved = $reservedPages | Sort-Object Address | Select-Object -First 1
$reservedStart = [uint32]$firstReserved.Address
$expectedMax = [uint32]($reservedStart - $appStart)

$mapLine = Select-String -LiteralPath $MapFile -Pattern '^\s*Execution Region ER_IROM1 ' | Select-Object -First 1
if (-not $mapLine) {
    throw "Cannot find ER_IROM1 summary in map file: $MapFile"
}

$regionMatch = [regex]::Match(
    $mapLine.Line,
    'Exec base:\s*(0x[0-9A-Fa-f]+).*Size:\s*(0x[0-9A-Fa-f]+).*Max:\s*(0x[0-9A-Fa-f]+)'
)

if (-not $regionMatch.Success) {
    throw "Cannot parse ER_IROM1 summary line: $($mapLine.Line)"
}

$execBase = [Convert]::ToUInt32($regionMatch.Groups[1].Value, 16)
$execSize = [Convert]::ToUInt32($regionMatch.Groups[2].Value, 16)
$execMax  = [Convert]::ToUInt32($regionMatch.Groups[3].Value, 16)
$appEnd   = [uint32]($execBase + $execSize)

if ($execBase -ne $appStart) {
    Write-Error ("FLASH layout mismatch: app base in map is 0x{0:X8}, but FLASH_ADDR_APP_START is 0x{1:X8}." -f $execBase, $appStart)
    exit 1
}

if ($execMax -ne $expectedMax) {
    Write-Error ("FLASH layout mismatch: ER_IROM1 Max is 0x{0:X8}, expected 0x{1:X8}. Check the Keil IROM region size." -f $execMax, $expectedMax)
    exit 1
}

if ($appEnd -gt $reservedStart) {
    Write-Error ("FLASH overlap detected: app end is 0x{0:X8}, first reserved page starts at 0x{1:X8}." -f $appEnd, $reservedStart)
    $reservedPages |
        Sort-Object Address |
        ForEach-Object {
            Write-Host ("Reserved page {0} at 0x{1:X8}" -f $_.Name, $_.Address)
        }
    exit 1
}

$margin = [uint32]($reservedStart - $appEnd)
Write-Host ("FLASH layout check passed: app_end=0x{0:X8}, reserved_start=0x{1:X8}, margin=0x{2:X} ({2} bytes)." -f $appEnd, $reservedStart, $margin)
$reservedPages |
    Sort-Object Address |
    ForEach-Object {
        Write-Host ("Reserved page {0} at 0x{1:X8}" -f $_.Name, $_.Address)
    }
