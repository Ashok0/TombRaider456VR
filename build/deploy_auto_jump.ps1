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
if ($builtHash -ne '9CC759DE3117B61FFE7CB3D006A39E36D072219D0CBB551F2652729CE05F50DF') {
    throw 'Release DLL does not match the tested auto-jump build.'
}
$previousInstalled = Test-Path -LiteralPath $installedDll
$iniHash = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
$backupDirectory = Join-Path $workspace ('build\before-auto-jump-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backupDirectory | Out-Null
foreach ($name in @('TombRaiderVR.dll', 'TombRaiderVR.ini', 'TombRaiderVR.log')) {
    $source = Join-Path $gameDirectory $name
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $backupDirectory }
}
if (-not $previousInstalled) {
    Copy-Item -LiteralPath (Join-Path $workspace 'build\auto-jump-previous.dll') -Destination (Join-Path $backupDirectory 'TombRaiderVR.dll')
}
Copy-Item -LiteralPath $builtDll -Destination $installedDll -Force
$installedHash = (Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash
if ($installedHash -ne $builtHash) { throw 'Installed DLL hash mismatch.' }
if ((Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash -ne $iniHash) {
    throw 'INI changed during deployment.'
}
$manifestDirectory = Join-Path $workspace 'build\auto-jump-deploy'
New-Item -ItemType Directory -Path $manifestDirectory -Force | Out-Null
$manifest = [ordered]@{
    status = 'deployed-and-hash-verified'
    deployed_at = (Get-Date).ToString('o')
    base_commit = 'e8f3321'
    dll_sha256 = $builtHash
    ini_sha256 = $iniHash
    backup = $backupDirectory
    destination = $gameDirectory
    ini_change = 'none'
    previous_dll_installed = $previousInstalled
    rollback_source = $(if ($previousInstalled) { 'previous installed DLL' } else { 'last confirmed DLL from e8f3321; installed DLL was absent' })
    regression_checks = [int]([regex]::Match((Get-Content -Raw (Join-Path $workspace 'build\auto-jump-release-tests.log')), 'OK: (\d+) first-person').Groups[1].Value)
    debug_regression_checks = [int]([regex]::Match((Get-Content -Raw (Join-Path $workspace 'build\auto-jump-debug.log')), 'OK: (\d+) first-person').Groups[1].Value)
    change = 'VR Modern Controls held forward Jump waits for a floor drop, accounting for seven native run-up frames and signed per-tick motion. Short release stays manual; long release and context changes cancel; takeoff consumes hold only when accepted. No VRIK solver changes. Native animation data verified across 55 TR4/5 tables. Headset validation pending.'
    hardware_verified = $false
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $manifestDirectory 'manifest.json') -Encoding UTF8
$manifest | ConvertTo-Json













