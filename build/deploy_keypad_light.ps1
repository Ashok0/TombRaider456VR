$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='4DA85395495BD708C82F5D895EAAB53A668D1BE8DE8EC93DC88716D5DE2C3374'
$expectedNew='A340D70400BBA2188053D1D9645164D80A0874289F0E33C6BBDE0A523B1F4813'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Close TR4-6 before installation; its DLL is locked.'}
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) {throw 'Tested build changed.'}
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) {throw 'Installed DLL changed.'}
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\keypad-test\before-button-light-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath $destination -Destination $backup
Copy-Item -LiteralPath $ini -Destination $backup
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Game restarted during preparation.'}
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) {throw 'Installed DLL verification failed.'}
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) {throw 'INI changed unexpectedly.'}
@{deployed=(Get-Date -Format o);backup=$backup;old=$expectedOld;new=$expectedNew;iniUnchanged=$true;
  keypadFocusedChecks=13056;keypadRealGLChecks=1576;mirrorFocusedChecks=19758;mirrorRealGLChecks=148;
  releaseBuildPassed=$true;fullSelfTestRun=$false;headsetTested=$false;
  behavior='TR5 first-person world keypad: touched button glows green for 350ms, including Clear/Enter; preserves native sounds and texture lettering; scoped draw and per-program reset';
  evidence='Full native draw fingerprints in both supported TR5 DLLs; actual keypad shader 9 tested on real GL driver, all 12 keys in raised/depressed states and unaffected panel/other keys; native input and mirror regressions passed'
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\keypad-test\light-deployment.json') -Encoding UTF8
Write-Output "Installed keypad button illumination: $expectedNew"
Write-Output "INI unchanged; previous DLL and INI backed up at $backup"
