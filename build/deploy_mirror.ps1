$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='14F01BB1F12D7145AEDA3653C84B9A322540C8CBC60A82AA8755F5C483F6E20A'
$expectedNew='C46F5A4B1CCB79AA24DA864DC5712ED25014FA62483A98754941BFEF592B15DB'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Close TR4-6 before installation; its DLL is locked.'}
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) {throw 'Tested build changed.'}
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) {throw 'Installed DLL changed.'}
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\mirror\before-install-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath $destination -Destination $backup
Copy-Item -LiteralPath $ini -Destination $backup
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Game restarted during preparation.'}
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) {throw 'Installed DLL verification failed.'}
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) {throw 'INI changed unexpectedly.'}
@{deployed=(Get-Date -Format o);backup=$backup;old=$expectedOld;new=$expectedNew;iniUnchanged=$true;
  mirrorFocusedChecks=2175;mirrorRealGLChecks=78;keypadChecks=10304;existingInteractionChecks=23563;
  releaseBuildPassed=$true;fullSelfTestRun=$false;headsetTested=$false;
  behavior='Prototype: TR5 RICH2 room 134 three audited bathroom panes, first-person Remastered Lara body/head/hair reflection, per-eye capture and depth-tested glass composite, current body pose reused; enabled by default'
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\mirror\deployment.json') -Encoding UTF8
Write-Output "Installed mirror prototype: $expectedNew"
Write-Output "INI unchanged; previous DLL and INI backed up at $backup"
