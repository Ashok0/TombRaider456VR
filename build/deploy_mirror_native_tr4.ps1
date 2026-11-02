$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='EF19022C551EC4591B3109F8FD02A11900CC661B9A3AB9BD5BAD869BA5A409C9'
$expectedNew='4DA85395495BD708C82F5D895EAAB53A668D1BE8DE8EC93DC88716D5DE2C3374'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Close TR4-6 before installation; its DLL is locked.'}
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) {throw 'Tested build changed.'}
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) {throw 'Installed DLL changed.'}
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\mirror\before-tr4-native-mirrors-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath $destination -Destination $backup
Copy-Item -LiteralPath $ini -Destination $backup
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Game restarted during preparation.'}
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) {throw 'Installed DLL verification failed.'}
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) {throw 'INI changed unexpectedly.'}
@{deployed=(Get-Date -Format o);backup=$backup;old=$expectedOld;new=$expectedNew;iniUnchanged=$true;
  mirrorFocusedChecks=19758;mirrorRealGLChecks=146;crouchTransitionChecks=3226;keypadChecks=10304;classicBodyChecks=551;nativeShadowAuditPassed=$true;
  releaseBuildPassed=$true;fullSelfTestRun=$false;headsetTested=$false;
  behavior='TR4 native Sacred Lake/Coastal Ruins mirror isolation in first person, preserved native mirror flag and untouched shadows; includes TR5 six-pane restroom support';
  evidence='All four supported DLL draw signatures verified offline; TR4 mirror prologues and relocation verified; tests cover Classic/HD, underwater hiding, native facial weights, physics isolation, state restoration and existing TR5 glass'
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\mirror\native-tr4-deployment.json') -Encoding UTF8
Write-Output "Installed combined TR4/TR5 mirror fixes: $expectedNew"
Write-Output "INI unchanged; previous DLL and INI backed up at $backup"
