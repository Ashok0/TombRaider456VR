$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='548E9F95C5630EB6B2AFEF65DA20959B0646ABB4F7B016CE4A0AFCEC9B8BBFDA'
$expectedNew='4E1E0CE4C26AEC723208DF8C9FD74422DD2FD7A538DA367B0F9D199D31B784E9'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Close TR4-6 before installing the wall-valve build.' }
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) { throw 'The tested build changed.' }
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) { throw 'Installed DLL changed; refusing to overwrite it.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\vr-options\before-wall-valves-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $destination,$ini -Destination $backup
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) { throw 'Installed DLL hash verification failed.' }
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI changed unexpectedly.' }
$record=@{
    deployed=(Get-Date -Format o);backup=$backup;dll=$expectedNew;ini_unchanged=$iniHash;
    release_build_passed=$true;valve_checks=3173;physical_switch_checks=2377;
    installed_valve_paths=3;rim_meshes_verified=2;native_control_layouts_verified=2;
    headset_tested=$false;full_suite_run=$false;
    change='TR5 airlock wall valves: Right Grip rim contact, circular rotation, release/regrab, native linked-item activation and post-use motion'
}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\vr-options\wall-valves-deployment.json') -Encoding UTF8
$record | ConvertTo-Json
