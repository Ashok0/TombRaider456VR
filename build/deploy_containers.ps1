$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='4E1E0CE4C26AEC723208DF8C9FD74422DD2FD7A538DA367B0F9D199D31B784E9'
$expectedNew='1C15429DAE54709640A2098CACB115E48B5BDBF768435E7209A5F59B68EC6E55'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Close TR4-6 before installing the drawer/cabinet build.' }
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) { throw 'The tested build changed.' }
$oldPresent=Test-Path -LiteralPath $destination
if($oldPresent -and (Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) { throw 'Installed DLL changed; refusing to overwrite it.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\vr-options\before-containers-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $ini -Destination $backup
if($oldPresent) { Copy-Item -LiteralPath $destination -Destination $backup }
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) { throw 'Installed DLL hash verification failed.' }
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI changed unexpectedly.' }
$record=@{
    deployed=(Get-Date -Format o);backup=$backup;dll=$expectedNew;ini_unchanged=$iniHash;previous_dll_present=$oldPresent;
    release_build_passed=$true;physical_interaction_checks=4491;container_asset_variants=8;native_layouts_verified=2;
    headset_tested=$false;full_suite_run=$false;
    change='TR5 searchable drawers and independent cabinet doors: Right Grip opening/closing, retained partial poses, visible handles and native pickup completion'
}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\vr-options\containers-deployment.json') -Encoding UTF8
$record | ConvertTo-Json
