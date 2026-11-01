$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='DC92021ED60725AF93165EA42577FA76512723E7EFCF60A3EC3E08C52D1E4664'
$expectedNew='368BF8FD67318669E342763829E9C71EE6B018A61E740FD6051604E02BBD5EEE'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Close TR4-6 before installing the VCI flip-lever build.' }
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) { throw 'The tested build changed.' }
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) { throw 'Installed DLL changed; refusing to overwrite it.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\vr-options\before-vci-levers-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $ini -Destination $backup
Copy-Item -LiteralPath $destination -Destination $backup
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Game started during preparation; installation cancelled.' }
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) { throw 'Installed DLL hash verification failed.' }
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI changed unexpectedly.' }
$record=@{
    deployed=(Get-Date -Format o);backup=$backup;dll=$expectedNew;previous_dll=$expectedOld;ini_unchanged=$iniHash;
    release_build_passed=$true;focused_interaction_checks=16838;vci_models=2;vci_directions=2;vci_wall_orientations=4;
    verified_vci_native_layouts=2;verified_vci_pose_frames=336;headset_tested=$false;full_suite_run=$false;
    change='TR5 VCI up/down levers use verified moving-bar contact and accurate partial regrips in first person; existing native activation, completion and timer handling preserved'
}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\vr-options\vci-lever-deployment.json') -Encoding UTF8
$record | ConvertTo-Json
