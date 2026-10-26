$ErrorActionPreference = 'Stop'
$workspace = 'C:\dev\TombRaider456VR'
$gameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$builtDll = Join-Path $workspace 'build\pistol-animation-release\TombRaiderVR.dll'
$installedDll = Join-Path $gameDirectory 'TombRaiderVR.dll'
$installedIni = Join-Path $gameDirectory 'TombRaiderVR.ini'
if (Get-Process | Where-Object { $_.ProcessName -match 'tomb|TombRaider' }) {
    throw 'Close the game before replacing the VR DLL.'
}
$builtHash = (Get-FileHash -LiteralPath $builtDll -Algorithm SHA256).Hash
if ($builtHash -ne '537F37F6CF1BB015000D5A27E134468E975B4F873C21AC383559A8ECEDE9192C') {
    throw 'Release DLL does not match the tested pistol-animation build.'
}
$iniHash = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
$backupDirectory = Join-Path $workspace ('build\before-pistol-animation-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
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
    regression_checks = 4247540
    hardware_verified = $false
    general_selftest = 'One pre-existing failure: full-body IK expected enabled but shipped template sets FirstPersonFullBodyIK=0.'
    change = 'First-person pistol draw/holster uses native animated hands and mesh timing, including physical holster requests. Full-body arm pose blends over 160 ms into animation and back to controllers; native bone reach and attachment wrist sharing retained. Head tracking and fitted body remain active. Other weapons, native animation speed, equip logic, physics and INI preserved.'
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $workspace 'build\pistol-animation-release\manifest.json') -Encoding UTF8
$manifest | ConvertTo-Json









