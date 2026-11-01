$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='EFCA14BF7B85708ED1597AAAD3D4FA28E1353A41A8BC54828406FD846DCD7664'
$expectedNew='14F01BB1F12D7145AEDA3653C84B9A322540C8CBC60A82AA8755F5C483F6E20A'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Close TR4-6 before installation; its DLL is locked.'}
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) {throw 'Tested build changed.'}
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) {throw 'Installed DLL changed.'}
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\keypad\before-install-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath $destination -Destination $backup
Copy-Item -LiteralPath $ini -Destination $backup
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Game restarted during preparation.'}
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) {throw 'Installed DLL verification failed.'}
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) {throw 'INI changed unexpectedly.'}
@{deployed=(Get-Date -Format o);backup=$backup;old=$expectedOld;new=$expectedNew;iniUnchanged=$true;
  keypadChecks=10304;existingInteractionChecks=23563;releaseBuildPassed=$true;
  fullSelfTestRun=$false;headsetTested=$false;
  behavior='TR5 VCI physical individual keypad keys in first-person Remastered with unarmed tracked hands; clear/zero/enter, native sounds and result sequence, world key lights; original Y menu retained'
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\keypad\deployment.json') -Encoding UTF8
Write-Output "Installed physical keypad build: $expectedNew"
Write-Output "INI unchanged; previous DLL and INI backed up at $backup"
