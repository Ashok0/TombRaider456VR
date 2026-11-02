$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='229E2C5317875D339C34368ADA56B96CFEC8158C1B0B5824D5CE04E2C0031151'
$expectedNew='DE0E1D1BC6572641247CA0F97AF5D987269A7141998484834A34B822ED692C17'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Close TR4-6 before installation; its DLL is locked.'}
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) {throw 'Tested build changed.'}
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) {throw 'Installed DLL changed.'}
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\mirror\before-face-fix-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath $destination -Destination $backup
Copy-Item -LiteralPath $ini -Destination $backup
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Game restarted during preparation.'}
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) {throw 'Installed DLL verification failed.'}
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) {throw 'INI changed unexpectedly.'}
@{deployed=(Get-Date -Format o);backup=$backup;old=$expectedOld;new=$expectedNew;iniUnchanged=$true;
  mirrorFocusedChecks=8502;mirrorRealGLChecks=79;crouchTransitionChecks=3226;keypadChecks=10304;
  releaseBuildPassed=$true;fullSelfTestRun=$false;headsetTested=$false;
  diagnosis='The head uses 26 facial helpers mapped to native IDs 39..64, outside the body pose cache and mesh visibility bits';
  behavior='Head descendants inherit fitted head correction with authored expressions intact; full facial skin palette visible; RICH2 mirrors also support third-person native pose and chase-camera distance'
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\mirror\face-fix-deployment.json') -Encoding UTF8
Write-Output "Installed mirror face and third-person fix: $expectedNew"
Write-Output "INI unchanged; previous DLL and INI backed up at $backup"
