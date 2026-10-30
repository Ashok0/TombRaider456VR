$ErrorActionPreference = 'Stop'
$workspace = 'C:\dev\TombRaider456VR'
$gameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$builtDll = Join-Path $workspace 'build\autojump-fp-resume-release\TombRaiderVR.dll'
$installedDll = Join-Path $gameDirectory 'TombRaiderVR.dll'
$installedIni = Join-Path $gameDirectory 'TombRaiderVR.ini'
if (Get-Process | Where-Object { $_.ProcessName -eq 'tomb456' -or ($_.Path -and $_.Path.StartsWith($gameDirectory + '\', [StringComparison]::OrdinalIgnoreCase)) }) {
    throw 'Close the game before replacing the VR DLL.'
}
$builtHash = (Get-FileHash -LiteralPath $builtDll -Algorithm SHA256).Hash
if ($builtHash -ne '387D37EFD6D86550BBABBA7F5C83DE087A48E25866678B6AF594B3B9DD2E2B4F') {
    throw 'Release DLL does not match the tested auto-jump camera build.'
}
$iniHash = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
$backupDirectory = Join-Path $workspace ('build\before-autojump-fp-resume-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
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
    regression_log = 'build\autojump-fp-resume-tests.log'
    address_verification = 'build\autojump-camera-addresses.log'
    hardware_verified = $false
    change = 'TR4/5 held running ledge jumps survive playable native camera changes in both view modes. First-person heading reacquisition preserves the held request, and playable overrides retain Modern Controls even with a saved Tank Controls preference. Both FP-specific failures reproduced in regressions before correction. Native control locks and actual cutscenes still cancel requests. Camera and third-person takeoff timing remain native. Release build, 4799055 regression checks and 814 address/binary checks passed.'
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $workspace 'build\autojump-fp-resume-release\manifest.json') -Encoding UTF8
$manifest | ConvertTo-Json

