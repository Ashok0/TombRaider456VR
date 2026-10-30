$ErrorActionPreference = 'Stop'
$workspace = 'C:\dev\TombRaider456VR'
$gameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$builtDll = Join-Path $workspace 'build\physical-crouch-release\TombRaiderVR.dll'
$installedDll = Join-Path $gameDirectory 'TombRaiderVR.dll'
$installedIni = Join-Path $gameDirectory 'TombRaiderVR.ini'
if (Get-Process | Where-Object { $_.ProcessName -eq 'tomb456' -or ($_.Path -and $_.Path.StartsWith($gameDirectory + '\', [StringComparison]::OrdinalIgnoreCase)) }) {
    throw 'Close the game before replacing the VR DLL.'
}
$builtHash = (Get-FileHash -LiteralPath $builtDll -Algorithm SHA256).Hash
if ($builtHash -ne 'A844430EAFAF2A82D8FC8621E1FCAEA9D1B7CF4AEFAAD407BA9FBDE5C5B2078C') { throw 'Unexpected release DLL.' }
$oldIniHash = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
if ($oldIniHash -ne 'E8DFF2DDF241CFACB44819D60346C2D9B754C9F7FC460036D2C43E49BED9C686') { throw 'INI changed since review.' }
$oldDllHash = (Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash
if ($oldDllHash -ne '27D093C654B28D488E5A30DC2373A2258EFBE2517AA987E27100E9CC6A1A5AAD') { throw 'Installed DLL changed since review.' }
$encoding = New-Object System.Text.UTF8Encoding($false)
$iniText = $encoding.GetString([System.IO.File]::ReadAllBytes($installedIni))
if ($iniText -match '(?m)^FirstPersonPhysicalCrouch=') { throw 'Physical crouch key unexpectedly exists.' }
$sections = [regex]::Matches($iniText, '(?m)^\[VR\]\r*\n')
if ($sections.Count -ne 1) { throw 'Expected one VR section.' }
$newline = if ($iniText.Contains("`r`n")) { "`r`n" } else { "`n" }
$nextIni = $iniText.Insert($sections[0].Index + $sections[0].Length, 'FirstPersonPhysicalCrouch=1' + $newline)
if ($nextIni -eq $iniText) { throw 'Setting insertion failed.' }
$backup = Join-Path $workspace ('build\before-physical-crouch-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
foreach ($name in @('TombRaiderVR.dll','TombRaiderVR.ini','TombRaiderVR.log')) {
    $source = Join-Path $gameDirectory $name
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $backup }
}
Copy-Item -LiteralPath $builtDll -Destination $installedDll -Force
[System.IO.File]::WriteAllBytes($installedIni, $encoding.GetBytes($nextIni))
if ((Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash -ne $builtHash) { throw 'Installed DLL hash mismatch.' }
if ($encoding.GetString([System.IO.File]::ReadAllBytes($installedIni)) -ne $nextIni) { throw 'Installed INI mismatch.' }
$manifest = [ordered]@{
    status = 'deployed-and-hash-verified'
    deployed_at = (Get-Date).ToString('o')
    dll_sha256 = $builtHash
    previous_dll_sha256 = $oldDllHash
    previous_ini_sha256 = $oldIniHash
    ini_sha256 = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
    backup = $backup
    setting_change = 'FirstPersonPhysicalCrouch=1 added; all other INI content preserved'
    validation = 'Release build passed; 102 standalone posture/input checks passed; native TR4/5 entry/exit code inspected; full self-test not run; headset unverified'
}
$json = $manifest | ConvertTo-Json -Depth 4
[System.IO.File]::WriteAllText((Join-Path $workspace 'build\physical-crouch-release\manifest.json'), $json, $encoding)
Write-Output $json
