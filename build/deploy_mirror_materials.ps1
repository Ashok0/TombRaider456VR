$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='74D3B34CD91C64982D219B130D13AA0819396DD1C41DBBAB4EEF14DE214F353C'
$expectedNew='EF19022C551EC4591B3109F8FD02A11900CC661B9A3AB9BD5BAD869BA5A409C9'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Close TR4-6 before installation; its DLL is locked.'}
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) {throw 'Tested build changed.'}
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) {throw 'Installed DLL changed.'}
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\mirror\before-materials-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath $destination -Destination $backup
Copy-Item -LiteralPath $ini -Destination $backup
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Game restarted during preparation.'}
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) {throw 'Installed DLL verification failed.'}
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) {throw 'INI changed unexpectedly.'}
@{deployed=(Get-Date -Format o);backup=$backup;old=$expectedOld;new=$expectedNew;iniUnchanged=$true;
  mirrorFocusedChecks=13665;mirrorRealGLChecks=122;crouchTransitionChecks=3226;keypadChecks=10304;
  releaseBuildPassed=$true;fullSelfTestRun=$false;headsetTested=$false;
  behavior='Capture all three native material passes with transparent hair coverage; compute rim and catsuit Fresnel from reflected eye position; native pose retained without IK';
  evidence='Shipped head has 2508 transparent and 2040 additive indices; actual native catsuit shader reproduces saturated pixels with the real camera and retains dark texture contrast with reflected observer'
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\mirror\materials-deployment.json') -Encoding UTF8
Write-Output "Installed mirror hairline/material fixes: $expectedNew"
Write-Output "INI unchanged; previous DLL and INI backed up at $backup"
