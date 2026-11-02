$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$source=Join-Path $root 'build\vr-options-release\TombRaiderVR.dll'
$destination=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$expectedOld='EF19022C551EC4591B3109F8FD02A11900CC661B9A3AB9BD5BAD869BA5A409C9'
$expectedNew='90AA4546E42335A62D51A1800AB27A1CCA2C9BDAE11453CCEBB226B2028F5BBB'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Close TR4-6 before installation; its DLL is locked.'}
if((Get-FileHash -LiteralPath $source).Hash -ne $expectedNew) {throw 'Tested build changed.'}
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedOld) {throw 'Installed DLL changed.'}
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\mirror\before-second-restroom-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath $destination -Destination $backup
Copy-Item -LiteralPath $ini -Destination $backup
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Game restarted during preparation.'}
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $expectedNew) {throw 'Installed DLL verification failed.'}
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) {throw 'INI changed unexpectedly.'}
@{deployed=(Get-Date -Format o);backup=$backup;old=$expectedOld;new=$expectedNew;iniUnchanged=$true;
  mirrorFocusedChecks=13683;mirrorRealGLChecks=146;crouchTransitionChecks=3226;keypadChecks=10304;
  releaseBuildPassed=$true;fullSelfTestRun=$false;headsetTested=$false;
  behavior='TR5 RICH2 rooms 134 and 135: six audited panes, native reflected animation in both views; per-room glass/plane selection with material pass isolation';
  evidence='Live room 135 metadata, 28 shipped glass triangles, OpenGL room-switch/occlusion/stereo tests; broader audit documented in README'
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\mirror\second-restroom-deployment.json') -Encoding UTF8
Write-Output "Installed second-restroom mirror fix: $expectedNew"
Write-Output "INI unchanged; previous DLL and INI backed up at $backup"
