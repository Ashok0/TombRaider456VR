$ErrorActionPreference='Stop'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$expectedOld='883F603FE5C2D7B2F3010B70FBB15A024525EC183E0E9FA0BD6D7E7C5C4E0527'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) {throw 'Game is running; close before installation.'}
$dll=Join-Path $game 'TombRaiderVR.dll';$ini=Join-Path $game 'TombRaiderVR.ini'
if((Get-FileHash -LiteralPath $dll).Hash -ne $expectedOld) {throw 'Installed height-fix checkpoint changed.'}
$release=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'release.json') -Raw | ConvertFrom-Json
$built=Join-Path $PSScriptRoot '456\bin\TombRaiderVR.dll'
if((Get-FileHash -LiteralPath $built).Hash -ne $release.SHA256) {throw 'Tested build changed.'}
$backup=Join-Path $PSScriptRoot ('before-install-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
foreach($name in @('TombRaiderVR.dll','TombRaiderVR.ini','TombRaiderVR.log')) {
 Copy-Item -LiteralPath (Join-Path $game $name) -Destination $backup
}
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
Copy-Item -LiteralPath $built -Destination $dll -Force
if((Get-FileHash -LiteralPath $dll).Hash -ne $release.SHA256) {throw 'Installed DLL mismatch.'}
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) {throw 'INI changed.'}
[pscustomobject]@{Installed=$dll;SHA256=$release.SHA256;Backup=$backup;INIUnchanged=$true;InHeadsetTested=$false} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'deployment.json') -Encoding UTF8
Write-Output 'Installed jump-camera fix. Previous height-fix DLL backed up; INI unchanged; installed hash verified.'
