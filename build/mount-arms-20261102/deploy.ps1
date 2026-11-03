param([ValidateSet('123','456')][string[]]$Collections = @('123','456'))
$ErrorActionPreference = 'Stop'
$menuRoot = $PSScriptRoot
$installations = @{
    '123' = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered'
    '456' = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
}
$processPattern = if ($Collections.Count -eq 2) { '^tomb|TombRaider' } elseif ($Collections[0] -eq '123') { '^tomb123$' } else { '^tomb456$' }
$runningGames = Get-Process | Where-Object { $_.ProcessName -match $processPattern }
if ($runningGames) { throw 'A Tomb Raider process is running; close it before deployment.' }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$records = @()
foreach ($collection in $Collections) {
    $gameDirectory = $installations[$collection]
    if (-not (Test-Path -LiteralPath $gameDirectory -PathType Container)) { throw "Missing game directory: $gameDirectory" }
    $ini = Join-Path $gameDirectory 'TombRaiderVR.ini'
    $iniBefore = (Get-FileHash -LiteralPath $ini -Algorithm SHA256).Hash
    foreach ($relative in @('TombRaiderVR.dll')) {
        $built = Join-Path "$menuRoot\$collection\bin" $relative
        $installed = Join-Path $gameDirectory $relative
        $backup = Join-Path "$menuRoot\before-deploy-$stamp\$collection" $relative
        if (Test-Path -LiteralPath $installed) {
            New-Item -ItemType Directory -Path (Split-Path $backup) -Force | Out-Null
            Copy-Item -LiteralPath $installed -Destination $backup
        }
        New-Item -ItemType Directory -Path (Split-Path $installed) -Force | Out-Null
        Copy-Item -LiteralPath $built -Destination $installed -Force
        $expected = (Get-FileHash -LiteralPath $built -Algorithm SHA256).Hash
        if ((Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash -ne $expected) { throw "Hash mismatch: $installed" }
        $records += [pscustomobject]@{ Collection=$collection; Installed=$installed; Backup=$backup; SHA256=$expected }
    }
    if ((Get-FileHash -LiteralPath $ini -Algorithm SHA256).Hash -ne $iniBefore) { throw "INI changed unexpectedly: $ini" }
}
$records | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$menuRoot\deployment-$($Collections -join '-')-$stamp.json" -Encoding UTF8
Write-Output "Installed collections $($Collections -join ', '); hashes verified; existing INIs unchanged."


