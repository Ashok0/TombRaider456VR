$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider123VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered'
$source=Join-Path $root 'build\button-touch\bin\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$old='78F8F4B6DE32AD0EB06D7FA69972CA703DEC310DC53B1CF6729A15264CD82C08'
$new='79CF34A1028BC6C7D8937C1828F6729A26B8F9C8383737AE40C8B9988EC088A8'
if(Get-Process tomb123 -ErrorAction SilentlyContinue) { throw 'Close TR1-3 before installing.' }
if((Get-FileHash -LiteralPath $source).Hash -ne $new) { throw 'Tested build changed.' }
if((Get-FileHash -LiteralPath $destination).Hash -ne $old) { throw 'Installed build changed.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\button-touch\before-install-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $destination -Destination $backup
Copy-Item -LiteralPath $ini -Destination $backup
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $new) { throw 'DLL verification failed.' }
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI changed unexpectedly.' }
$record=@{
    deployed=(Get-Date -Format o);dll=$new;backup=$backup;ini_unchanged=$iniHash;
    release_build_passed=$true;focused_interaction_checks=8912;button_profiles_verified=14;
    level_data_files_verified=62;push_button_native_layouts_verified=6;
    headset_tested=$false;full_suite_run=$false;
    change='TR2/3 push-button touch with either controller, both idle faces, native timers and triggers; TR1 grip levers preserved'
}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\button-touch\deployment.json') -Encoding UTF8
$record | ConvertTo-Json
