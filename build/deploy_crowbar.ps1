$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='368BF8FD67318669E342763829E9C71EE6B018A61E740FD6051604E02BBD5EEE'
$expectedNew='339BDDF85C90D6AC71E41FF989D8346DF4E44E11E389E89CEA4FAA01EE7E38BF'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Close TR4-6 before installing the crowbar build.' }
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) { throw 'The tested build changed.' }
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) { throw 'Installed DLL changed; refusing to overwrite it.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\vr-options\before-crowbar-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $ini -Destination $backup
Copy-Item -LiteralPath $destination -Destination $backup
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Game started during preparation; installation cancelled.' }
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) { throw 'Installed DLL hash verification failed.' }
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI changed unexpectedly.' }
$record=@{
    deployed=(Get-Date -Format o);backup=$backup;dll=$expectedNew;previous_dll=$expectedOld;ini_unchanged=$iniHash;
    release_build_passed=$true;focused_interaction_checks=23563;pulley_checks=1665;cog_checks=2508;
    verified_crowbar_binaries=2;verified_crowbar_level_assets=3;headset_tested=$false;full_suite_run=$false;
    change='TR4 native crowbar insertion followed by physical RG downward pry; release holds pose; native failures, removal, effects and completion retained'
}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\vr-options\crowbar-deployment.json') -Encoding UTF8
$record | ConvertTo-Json
