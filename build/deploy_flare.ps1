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
if ($builtHash -ne 'D551A915CAC0BB88EB1F464BA197BB4ADB3362CB84D156BE0045A6D26949E81D') {
    throw 'Release DLL does not match the tested flare build.'
}
$iniHash = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
$backupDirectory = Join-Path $workspace ('build\before-flare-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
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
$manifestDirectory = Join-Path $workspace 'build\flare-deploy'
New-Item -ItemType Directory -Path $manifestDirectory -Force | Out-Null
$manifest = [ordered]@{
    status = 'deployed-and-hash-verified'
    deployed_at = (Get-Date).ToString('o')
    base_commit = 'd4e2833'
    dll_sha256 = $builtHash
    ini_sha256 = $iniHash
    backup = $backupDirectory
    destination = $gameDirectory
    ini_change = 'none'
    regression_checks = [int]([regex]::Match((Get-Content -Raw (Join-Path $workspace 'build\flare-release-tests.log')), 'OK: (\d+) first-person').Groups[1].Value)
    debug_regression_checks = [int]([regex]::Match((Get-Content -Raw (Join-Path $workspace 'build\flare-debug.log')), 'OK: (\d+) first-person').Groups[1].Value)
    change = 'First-person flare turning and left-controller hand IK in TR4/5. Native held-flare update receives tracked current/previous left wrist positions for light interpolation and mirrors. Full-body facing falls back when the follower is unavailable. Native ignition, burn-out, draw/throw, firearms and holsters retained. New draw/IK/light regression cases and four-DLL hook verification passed; headset validation pending.'
    hardware_verified = $false
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $manifestDirectory 'manifest.json') -Encoding UTF8
$manifest | ConvertTo-Json













