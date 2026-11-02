$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='DE0E1D1BC6572641247CA0F97AF5D987269A7141998484834A34B822ED692C17'
$expectedNew='74D3B34CD91C64982D219B130D13AA0819396DD1C41DBBAB4EEF14DE214F353C'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Close TR4-6 before installation; its DLL is locked.'}
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) {throw 'Tested build changed.'}
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) {throw 'Installed DLL changed.'}
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\mirror\before-native-pose-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath $destination -Destination $backup
Copy-Item -LiteralPath $ini -Destination $backup
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Game restarted during preparation.'}
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) {throw 'Installed DLL verification failed.'}
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) {throw 'INI changed unexpectedly.'}
@{deployed=(Get-Date -Format o);backup=$backup;old=$expectedOld;new=$expectedNew;iniUnchanged=$true;
  mirrorFocusedChecks=13642;mirrorRealGLChecks=79;crouchTransitionChecks=3226;keypadChecks=10304;
  releaseBuildPassed=$true;fullSelfTestRun=$false;headsetTested=$false;
  behavior='Both camera modes reflect native third-person animation without VR IK, room-scale body offsets, tracked hand substitutions, or physical holster overrides; ordinary first-person tracking retained'
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\mirror\native-pose-deployment.json') -Encoding UTF8
Write-Output "Installed native-pose mirror fix: $expectedNew"
Write-Output "INI unchanged; previous DLL and INI backed up at $backup"
