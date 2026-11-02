$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='C46F5A4B1CCB79AA24DA864DC5712ED25014FA62483A98754941BFEF592B15DB'
$expectedNew='229E2C5317875D339C34368ADA56B96CFEC8158C1B0B5824D5CE04E2C0031151'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Close TR4-6 before installation; its DLL is locked.'}
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) {throw 'Tested build changed.'}
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) {throw 'Installed DLL changed.'}
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\mirror\before-room-fix-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath $destination -Destination $backup
Copy-Item -LiteralPath $ini -Destination $backup
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Game restarted during preparation.'}
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) {throw 'Installed DLL verification failed.'}
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) {throw 'INI changed unexpectedly.'}
@{deployed=(Get-Date -Format o);backup=$backup;old=$expectedOld;new=$expectedNew;iniUnchanged=$true;
  mirrorFocusedChecks=2176;mirrorRealGLChecks=78;releaseBuildPassed=$true;fullSelfTestRun=$false;headsetTested=$false;
  diagnosis='Live TR5 RICH2 native number_rooms=179, unlike 180 HD TRG records; v1 rejected the scene before capture allocation';
  behavior='Use verified native room count in both mirror gates; keep exact room/origin/glass range checks; log first body capture and stereo composite'
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\mirror\room-fix-deployment.json') -Encoding UTF8
Write-Output "Installed mirror room fix: $expectedNew"
Write-Output "INI unchanged; previous DLL and INI backed up at $backup"
