$ErrorActionPreference = 'Stop'
$workspace = 'C:\dev\TombRaider456VR'
$gameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$builtDll = Join-Path $workspace 'build\roomscale-native-release\TombRaiderVR.dll'
$installedDll = Join-Path $gameDirectory 'TombRaiderVR.dll'
$installedIni = Join-Path $gameDirectory 'TombRaiderVR.ini'
if (Get-Process | Where-Object { $_.ProcessName -match 'tomb|TombRaider' }) {
    throw 'Close the game before replacing the VR DLL.'
}
$builtHash = (Get-FileHash -LiteralPath $builtDll -Algorithm SHA256).Hash
if ($builtHash -ne '258C5107B735E5240C811AE52CEFC6056A255CE5A9704BAF4F28B8B2E0637B0C') {
    throw 'Release DLL does not match the tested roomscale-native build.'
}
$iniHash = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
$backupDirectory = Join-Path $workspace ('build\before-roomscale-native-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
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
    regression_checks = 4304961
    hardware_verified = $false
    change = 'Corrects the native standing collision pass that undid roomscale steps near ledges. Verified global Lara collision record and scoped GetCollisionInfo override preserve wall and joystick rules. Eye clearance permits deep drops. Revised regression reproduces the previous failure and covers native pushback, falling, landing, camera movement and exclusion gates.'
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $workspace 'build\roomscale-native-release\manifest.json') -Encoding UTF8
$manifest | ConvertTo-Json


