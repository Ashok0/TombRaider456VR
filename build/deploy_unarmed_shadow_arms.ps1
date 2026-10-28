$ErrorActionPreference = 'Stop'
$workspace = 'C:\dev\TombRaider456VR'
$gameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$builtDll = Join-Path $workspace 'build\world-laser-release\TombRaiderVR.dll'
$installedDll = Join-Path $gameDirectory 'TombRaiderVR.dll'
$installedIni = Join-Path $gameDirectory 'TombRaiderVR.ini'
if (Get-Process | Where-Object { $_.ProcessName -match 'tomb|TombRaider' }) {
    throw 'Close the game before replacing the VR DLL.'
}
$builtHash = (Get-FileHash -LiteralPath $builtDll -Algorithm SHA256).Hash
if ($builtHash -ne '40375825BC89302BC6D3522CC46C055BA437EC94CAC47DB8A81A1036A0E49C68') {
    throw 'Release DLL does not match the tested world-laser build.'
}
$iniHash = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
$backupDirectory = Join-Path $workspace ('build\before-world-laser-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
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
    regression_log = 'build\world-laser-regression.log'
    hardware_verified = $false
    change = 'Unarmed first-person shadow arms follow motion controllers through arm-only IK in the light palette. Excludes eye-view body/wall offsets and preserves native root, torso, head, legs, draw masks, gameplay state, and eye pose caches. Tests cover 15/33-joint meshes, real DrawCreatureHD path, full-body IK toggle, light-camera translation, independent hand movement, third person and tracking-loss/native-interaction fallbacks. Release build and 4333252 first-person regression checks passed.'
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $workspace 'build\world-laser-release\manifest.json') -Encoding UTF8
$manifest | ConvertTo-Json











