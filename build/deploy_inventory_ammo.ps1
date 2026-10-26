$ErrorActionPreference = 'Stop'
$workspace = 'C:\dev\TombRaider456VR'
$gameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$builtDll = Join-Path $workspace 'build\inventory-ammo-release\TombRaiderVR.dll'
$installedDll = Join-Path $gameDirectory 'TombRaiderVR.dll'
$installedIni = Join-Path $gameDirectory 'TombRaiderVR.ini'
if (Get-Process | Where-Object { $_.ProcessName -match 'tomb|TombRaider' }) {
    throw 'Close the game before replacing the VR DLL.'
}
$builtHash = (Get-FileHash -LiteralPath $builtDll -Algorithm SHA256).Hash
if ($builtHash -ne '91A68120C5FD92E60739B6A1ACB0DDF99DE3E4A0A60CF46D79BE60C2F7F1C0F3') {
    throw 'Release DLL does not match the tested inventory build.'
}
$iniHash = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
$backupDirectory = Join-Path $workspace ('build\before-world-inventory-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
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
    regression_checks = 4169082
    hardware_verified = $false
    general_selftest = 'One pre-existing failure: full-body IK expected enabled but shipped template sets FirstPersonFullBodyIK=0.'
    change = 'TR4/5 Touch X/BACK opens native paused inventory anchored to opening headset pose; armed X is consumed before menu handling and queues one native DoHotkeys ammo change per press. Drawing/holstering blocks inventory; single-ammo weapons do nothing. Actual native ammo cycling, wrap, cooldown, feedback and readiness guards verified in isolated memory for all four TR4/5 binaries. R3+LS down no longer opens spatial inventory. LS navigates, A selects/uses, B backs/closes. Live stereo level rendering replaces native captured background; paused first-person camera preserved. 66 real OpenGL scene checks and all four binary hook verifications passed. Entry/exit input drains begin only after native inventory opens; other movement and firing inputs remain untouched. Photo Mode unchanged. Existing INI preserved.'
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $workspace 'build\inventory-ammo-release\manifest.json') -Encoding UTF8
$manifest | ConvertTo-Json



