$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='A0B31A9B60CE710E59085A70F13190C2A9CB08E98E72006DB5582E86DBC91F34'
$expectedNew='FF08FAC8004D3B84824B62B8902CC8B6917ED1CF18E392F7DF9479E791EBEEBE'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Close TR4-6 before installing the button-touch build.' }
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) { throw 'The tested build changed.' }
$oldPresent=Test-Path -LiteralPath $destination
if($oldPresent -and (Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) { throw 'Installed DLL changed; refusing to overwrite it.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\vr-options\before-button-touch-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $ini -Destination $backup
if($oldPresent) { Copy-Item -LiteralPath $destination -Destination $backup }
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) { throw 'Installed DLL hash verification failed.' }
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI changed unexpectedly.' }
$record=@{
    deployed=(Get-Date -Format o);backup=$backup;dll=$expectedNew;ini_unchanged=$iniHash;previous_dll_present=$oldPresent;
    release_build_passed=$true;physical_interaction_checks=10296;button_profiles_verified=21;installed_button_poses_verified=18;native_layouts_verified=4;
    headset_tested=$false;full_suite_run=$false;
    change='TR4/5 wall, elevator and sequence buttons: either controller touches the button face to activate native button control, preserving timers, puzzle ordering and used-state restrictions'
}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\vr-options\button-touch-deployment.json') -Encoding UTF8
$record | ConvertTo-Json

