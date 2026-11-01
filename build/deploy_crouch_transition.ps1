$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='FF08FAC8004D3B84824B62B8902CC8B6917ED1CF18E392F7DF9479E791EBEEBE'
$expectedNew='73AE605DC8AF0445BC3EE4B99133929577E41A2581499B2DE139B23D2311BBEE'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Close TR4-6 before installing the crouch-transition build.' }
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) { throw 'The tested build changed.' }
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) { throw 'Installed DLL changed; refusing to overwrite it.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\vr-options\before-crouch-transition-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $ini -Destination $backup
Copy-Item -LiteralPath $destination -Destination $backup
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) { throw 'Installed DLL hash verification failed.' }
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI changed unexpectedly.' }
$record=@{
    deployed=(Get-Date -Format o);backup=$backup;dll=$expectedNew;previous_dll=$expectedOld;ini_unchanged=$iniHash;
    release_build_passed=$true;focused_crouch_transition_checks=3226;
    headset_tested=$false;full_suite_run=$false;
    change='TR4/5 first-person crawl keeps tracked hands and headset-relative steering during ordinary queued crouch transitions; native posture queue, hands-busy status, ceiling clearance and interaction guards preserved'
}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\vr-options\crouch-transition-deployment.json') -Encoding UTF8
$record | ConvertTo-Json
