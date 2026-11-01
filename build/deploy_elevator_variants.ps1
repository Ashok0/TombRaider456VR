$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='9559822C69A3D27703D7244D3DAF15216AA0B41170C872828FF744F5407631B7'
$expectedNew='02D6761D3AE4EB14C57B100C8D6B144A0B94430B536785F3D11F8615EEE39998'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Close TR4-6 before installing the interior elevator panel update.' }
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) { throw 'The tested build changed.' }
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) { throw 'Installed DLL changed; refusing to overwrite it.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\vr-options\before-elevator-variants-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $ini -Destination $backup
Copy-Item -LiteralPath $destination -Destination $backup
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) { throw 'Installed DLL hash verification failed.' }
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI changed unexpectedly.' }
$record=@{
    deployed=(Get-Date -Format o);backup=$backup;dll=$expectedNew;previous_dll=$expectedOld;ini_unchanged=$iniHash;
    release_build_passed=$true;focused_interaction_checks=11730;button_profiles=22;installed_button_poses=20;native_button_layouts=4;native_lift_layouts=2;
    headset_tested=$false;full_suite_run=$false;
    change='Accept TR5 keypad panels OCB 5/6/7; only OCB 6 requires the preset-trip route and native travel lock; reproduced live OCB 5 rejection before correction'
}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\vr-options\elevator-variants-deployment.json') -Encoding UTF8
$record | ConvertTo-Json

