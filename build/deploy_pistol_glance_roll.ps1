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
if ($builtHash -ne 'DE9AB89529C164DF703A6486536255C4BADF1B610C07429317EC68BC2BB8488F') {
    throw 'Release DLL does not match the tested pistol-glance-roll build.'
}
$iniHash = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
$backupDirectory = Join-Path $workspace ('build\before-pistol-glance-roll-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
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
$manifestDirectory = Join-Path $workspace 'build\pistol-glance-roll-deploy'
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
    regression_checks = [int]([regex]::Match((Get-Content -Raw (Join-Path $workspace 'build\pistol-glance-release-tests.log')), 'OK: (\d+) first-person').Groups[1].Value)
    debug_regression_checks = [int]([regex]::Match((Get-Content -Raw (Join-Path $workspace 'build\pistol-glance-debug.log')), 'OK: (\d+) first-person').Groups[1].Value)
    change = 'Pistol-only material roll continuity around ill-conditioned wrist swing. Transports controller rotation and bone direction; bounds reconciliation by input motion and prevents repeated draw updates. Exact tracked wrist and existing unarmed solver retained. Mirrored shoulder-glance regression fails without protection and passes with it. Runtime logs report minimum twist confidence and recovery. Debug and optimized VR suites passed. Headset cause still requires confirmation.'
    hardware_verified = $false
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $manifestDirectory 'manifest.json') -Encoding UTF8
$manifest | ConvertTo-Json













