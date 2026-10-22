$ErrorActionPreference = 'Stop'
$workspace = 'C:\dev\TombRaider456VR'
$gameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$builtDll = Join-Path $workspace 'build\x64\Release\TombRaiderVR.dll'
$installedDll = Join-Path $gameDirectory 'TombRaiderVR.dll'
$installedIni = Join-Path $gameDirectory 'TombRaiderVR.ini'
if (Get-Process | Where-Object { $_.ProcessName -match 'tomb|TombRaider' }) {
    throw 'Close the game before replacing the VR DLL.'
}
$builtHash = (Get-FileHash -LiteralPath $builtDll -Algorithm SHA256).Hash
if ($builtHash -ne '0600C80CF3BE178316EE3684321C80A0A66AB8DD795E4030BE94C9C695057536') {
    throw 'Release DLL does not match the tested rigid-arm-reach build.'
}
$iniHash = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
$backupDirectory = Join-Path $workspace ('build\before-rigid-arm-reach-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backupDirectory | Out-Null
foreach ($name in @('TombRaiderVR.dll', 'TombRaiderVR.ini', 'TombRaiderVR.log')) {
    $source = Join-Path $gameDirectory $name
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $backupDirectory }
}
Copy-Item -LiteralPath $builtDll -Destination $installedDll -Force
$installedHash = (Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash
if ($installedHash -ne $builtHash) { throw 'Installed DLL hash mismatch.' }
if ((Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash -ne $iniHash) {
    throw 'INI changed during deployment.'
}
$manifestDirectory = Join-Path $workspace 'build\rigid-arm-reach-deploy'
New-Item -ItemType Directory -Path $manifestDirectory -Force | Out-Null
$manifest = [ordered]@{
    status = 'deployed-and-hash-verified'
    deployed_at = (Get-Date).ToString('o')
    base_commit = '22cb815'
    dll_sha256 = $builtHash
    ini_sha256 = $iniHash
    backup = $backupDirectory
    destination = $gameDirectory
    ini_change = 'none'
    regression_checks = 3415961
    debug_regression_checks = 3415961
    change = 'Preserve tracked upper-arm and forearm mesh lengths; translate visual shoulder by excess reach while retaining exact controller wrists and gun poses. Native grips and legs retain existing solver behavior. Regression coverage checks reach boundaries, connected rigid meshes, stereo determinism, calibrated scales, both TR4/5 palette layouts and full-body and arm-only rendering; prior camera, mounting and visibility regressions retained.'
    hardware_verified = $false
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $manifestDirectory 'manifest.json') -Encoding UTF8
$manifest | ConvertTo-Json








