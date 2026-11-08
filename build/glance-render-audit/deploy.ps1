$ErrorActionPreference='Stop'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$validation=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'validation.json') -Raw | ConvertFrom-Json
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Game is running. No installation performed.' }
$built=Join-Path $PSScriptRoot '456\bin\TombRaiderVR.dll'
$installed=Join-Path $game 'TombRaiderVR.dll'
if((Get-FileHash -LiteralPath $built).Hash -ne $validation.builtDLL) { throw 'Built DLL changed.' }
if((Get-FileHash -LiteralPath $installed).Hash -ne $validation.previousDLL) { throw 'Installed DLL changed.' }
$backup=Join-Path $PSScriptRoot ('before-install-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $installed -Destination $backup
$ini=Join-Path $game 'TombRaiderVR.ini'
Copy-Item -LiteralPath $ini -Destination $backup
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
Copy-Item -LiteralPath $built -Destination $installed -Force
if((Get-FileHash -LiteralPath $installed).Hash -ne $validation.builtDLL) { throw 'Installation hash mismatch.' }
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI changed unexpectedly.' }
[pscustomobject]@{Installed=$installed;SHA256=$validation.builtDLL;Backup=$backup;INIUnchanged=$true;DiagnosticOnly=$true;TR123Unchanged=$true} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'deployment.json') -Encoding UTF8
Write-Output 'TR4-6 diagnostic installed and hash verified. Previous DLL backed up; INI and TR1-3 unchanged.'
