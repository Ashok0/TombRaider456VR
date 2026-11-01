$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='1C15429DAE54709640A2098CACB115E48B5BDBF768435E7209A5F59B68EC6E55'
$expectedNew='A0B31A9B60CE710E59085A70F13190C2A9CB08E98E72006DB5582E86DBC91F34'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Close TR4-6 before installing the TV screen-touch build.' }
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) { throw 'The tested build changed.' }
$oldPresent=Test-Path -LiteralPath $destination
if($oldPresent -and (Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) { throw 'Installed DLL changed; refusing to overwrite it.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\vr-options\before-monitor-touch-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $ini -Destination $backup
if($oldPresent) { Copy-Item -LiteralPath $destination -Destination $backup }
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) { throw 'Installed DLL hash verification failed.' }
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI changed unexpectedly.' }
$record=@{
    deployed=(Get-Date -Format o);backup=$backup;dll=$expectedNew;ini_unchanged=$iniHash;previous_dll_present=$oldPresent;
    release_build_passed=$true;physical_interaction_checks=4965;monitor_levels_verified=2;native_layouts_verified=2;
    headset_tested=$false;full_suite_run=$false;
    change='TR5 interactive monitors: either controller touches screen glass to activate the native one-shot level interaction without Action or scripted body alignment'
}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\vr-options\monitor-touch-deployment.json') -Encoding UTF8
$record | ConvertTo-Json
