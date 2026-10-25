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
if ($builtHash -ne 'B420599624A4E90F6C7535ABE4B2D7F1BEBF8CBFD003FB3DA00C420658BDA613') {
    throw 'Release DLL does not match the tested flare build.'
}
$iniHash = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
$backupDirectory = Join-Path $workspace ('build\before-vr-buttons-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
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
$manifestDirectory = Join-Path $workspace 'build\vr-buttons-deploy'
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
    regression_checks = [int]([regex]::Match((Get-Content -Raw (Join-Path $workspace 'build\vr-buttons-release-tests.log')), 'OK: (\d+) first-person').Groups[1].Value)
    debug_regression_checks = [int]([regex]::Match((Get-Content -Raw (Join-Path $workspace 'build\vr-buttons-debug.log')), 'OK: (\d+) first-person').Groups[1].Value)
    change = 'TR4/5 VR background controller polling: scope native isActive override after message pump and restore on inputUpdate return; native activation/keyboard/mouse message handling remains unchanged. Face buttons, grips, sticks, release and focus-transition regressions pass. Stock/retail EXE hook boundaries, RIP relocation, message call, focus gate and XInput path verified. Prior independent triggers, flare light/crash and VRIK fixes retained. Headset validation pending.'
    hardware_verified = $false
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $manifestDirectory 'manifest.json') -Encoding UTF8
$manifest | ConvertTo-Json













