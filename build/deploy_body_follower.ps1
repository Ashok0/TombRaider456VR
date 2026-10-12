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
if ($builtHash -ne '3855CFA61596BEBA7A47F68FB428E48BC47107D326E86249504D44F2E602AC28') {
    throw 'Release DLL does not match the tested full-body follower build.'
}
$iniHash = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
$byteEncoding = [System.Text.Encoding]::GetEncoding(28591)
$iniBytes = [System.IO.File]::ReadAllBytes($installedIni)
$iniText = $byteEncoding.GetString($iniBytes)
$bodySetting = [regex]'(?m)^(FirstPersonFullBodyIK[ \t]*=[ \t]*)[01]([ \t]*\r?)$'
if ($bodySetting.Matches($iniText).Count -ne 1) {
    throw 'Expected exactly one installed FirstPersonFullBodyIK setting.'
}
$newIniText = $bodySetting.Replace($iniText, '${1}1${2}')
$backupDirectory = Join-Path $workspace ('build\before-body-follower-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backupDirectory | Out-Null
foreach ($name in @('TombRaiderVR.dll', 'TombRaiderVR.ini', 'TombRaiderVR.log')) {
    $source = Join-Path $gameDirectory $name
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $backupDirectory }
}
if ((Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash -ne $iniHash) {
    throw 'INI changed during backup; deployment cancelled.'
}
Copy-Item -LiteralPath $builtDll -Destination $installedDll -Force
[System.IO.File]::WriteAllBytes($installedIni, $byteEncoding.GetBytes($newIniText))
$installedHash = (Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash
if ($installedHash -ne $builtHash) { throw 'Installed DLL hash mismatch.' }
if ($byteEncoding.GetString([System.IO.File]::ReadAllBytes($installedIni)) -cne $newIniText) {
    throw 'Installed INI did not match the intended single-setting change.'
}
$manifestDirectory = Join-Path $workspace 'build\body-follower-deploy'
New-Item -ItemType Directory -Path $manifestDirectory -Force | Out-Null
$manifest = [ordered]@{
    status = 'deployed-and-hash-verified'
    deployed_at = (Get-Date).ToString('o')
    base_commit = '898efed'
    source_state = 'working tree with full-body follower and holster hover changes'
    dll_sha256 = $builtHash
    ini_before_sha256 = $iniHash
    ini_after_sha256 = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
    backup = $backupDirectory
    destination = $gameDirectory
    ini_change = 'FirstPersonFullBodyIK=1; all other bytes preserved'
    regression_checks = 1694568
    debug_regression_checks = 1694568
    body_yaw_slack_degrees = 45
    lean_radius_metres = 0.15
    automatic_game_crouch_default = $false
    hardware_verified = $false
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $manifestDirectory 'manifest.json') -Encoding UTF8
$manifest | ConvertTo-Json
