$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='5BC258741382F1DD858E37A3F5032564C2628FF97048CAD131A9E2D1AB13F76A'
$expectedNew='DC92021ED60725AF93165EA42577FA76512723E7EFCF60A3EC3E08C52D1E4664'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Close TR4-6 before installing the pulley interaction build.' }
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) { throw 'The tested build changed.' }
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) { throw 'Installed DLL changed; refusing to overwrite it.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\vr-options\before-pulleys-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $ini -Destination $backup
Copy-Item -LiteralPath $destination -Destination $backup
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Game started during preparation; installation cancelled.' }
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) { throw 'Installed DLL hash verification failed.' }
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI changed unexpectedly.' }
$record=@{
    deployed=(Get-Date -Format o);backup=$backup;dll=$expectedNew;previous_dll=$expectedOld;ini_unchanged=$iniHash;
    release_build_passed=$true;focused_pulley_checks=1665;existing_switch_checks=11730;existing_cog_checks=2508;
    verified_native_layouts=4;verified_pulley_animation_sets=12;headset_tested=$false;full_suite_run=$false;
    change='TR4/5 first-person RG downward pulls for verified native PULLEY mechanism chains/ropes; native count/latch/one-use/floor triggers and timers preserved; locked/special/traversal/trap variants remain native'
}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\vr-options\pulley-deployment.json') -Encoding UTF8
$record | ConvertTo-Json
