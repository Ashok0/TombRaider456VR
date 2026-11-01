$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider123VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered'
$source=Join-Path $root 'build\crouch-transition\bin\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$old='ED80723AC267DA94446AC6BD7C17EF6189397A4C18D0C1D95B2BB4A050D148F1'
$new='BB7216B29E71E230296284D42A02EC5954EA46C46A603A429FB7E39BC1193B1D'
if(Get-Process tomb123 -ErrorAction SilentlyContinue) { throw 'Close TR1-3 before installing.' }
if((Get-FileHash -LiteralPath $source).Hash -ne $new) { throw 'Tested build changed.' }
if((Get-FileHash -LiteralPath $destination).Hash -ne $old) { throw 'Installed build changed.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\crouch-transition\before-install-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $destination -Destination $backup
Copy-Item -LiteralPath $ini -Destination $backup
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $new) { throw 'DLL verification failed.' }
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI changed unexpectedly.' }
$record=@{
    deployed=(Get-Date -Format o);dll=$new;previous_dll=$old;backup=$backup;ini_unchanged=$iniHash;
    release_build_passed=$true;focused_crouch_transition_checks=5490;
    headset_tested=$false;full_suite_run=$false;
    change='TR3 first-person queued crouch/crawl transitions retain headset-relative steering and controller hands during native hands-busy status; TR1/2 physical posture preserved'
}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\crouch-transition\deployment.json') -Encoding UTF8
$record | ConvertTo-Json
