$ErrorActionPreference = 'Stop'
$workspace = 'C:\dev\TombRaider456VR'
$gameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$builtDll = Join-Path $workspace 'build\roomscale-ledge-release\TombRaiderVR.dll'
$installedDll = Join-Path $gameDirectory 'TombRaiderVR.dll'
$installedIni = Join-Path $gameDirectory 'TombRaiderVR.ini'
if (Get-Process | Where-Object { $_.ProcessName -match 'tomb|TombRaider' }) {
    throw 'Close the game before replacing the VR DLL.'
}
$builtHash = (Get-FileHash -LiteralPath $builtDll -Algorithm SHA256).Hash
if ($builtHash -ne '141A566AAC828DFDC2DCBBECBF7C0E7312DB862F157D5AFB2B714C1BCF441D67') {
    throw 'Release DLL does not match the tested roomscale-ledge build.'
}
$iniHash = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
$backupDirectory = Join-Path $workspace ('build\before-roomscale-ledge-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backupDirectory | Out-Null
foreach ($name in @('TombRaiderVR.dll', 'TombRaiderVR.ini', 'TombRaiderVR.log')) {
    $source = Join-Path $gameDirectory $name
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $backupDirectory }
}
Copy-Item -LiteralPath $builtDll -Destination $installedDll -Force
if ((Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash -ne $builtHash) {
    throw 'Installed DLL hash mismatch.'
}
if ((Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash -ne $iniHash) {
    throw 'INI changed during deployment.'
}
$manifest = [ordered]@{
    status = 'deployed-and-hash-verified'
    deployed_at = (Get-Date).ToString('o')
    dll_sha256 = $builtHash
    ini_sha256 = $iniHash
    backup = $backupDirectory
    destination = $gameDirectory
    regression_checks = 4276658
    hardware_verified = $false
    change = 'Roomscale sweeps allow ledge crossings, stop at the first unsupported step, and hand off falling to native physics. Airborne gravity blocks further root dragging. Regression coverage includes TR4/TR5 directional drops, camera descent, landing, walls, missing geometry, full-body IK and stabilization.'
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $workspace 'build\roomscale-ledge-release\manifest.json') -Encoding UTF8
$manifest | ConvertTo-Json

