$ErrorActionPreference='Stop'
$release=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'release.json') -Raw | ConvertFrom-Json
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Close the game before installation.' }
$built=Join-Path $PSScriptRoot '456\bin\TombRaiderVR.dll'
$installed=Join-Path $release.Directory 'TombRaiderVR.dll'
$ini=Join-Path $release.Directory 'TombRaiderVR.ini'
if((Get-FileHash -LiteralPath $built).Hash -ne $release.Expected) { throw 'Tested DLL changed.' }
if((Get-FileHash -LiteralPath $installed).Hash -ne $release.Previous) { throw 'Installed DLL changed.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $PSScriptRoot ('before-install-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $installed -Destination $backup
Copy-Item -LiteralPath $ini -Destination $backup
Copy-Item -LiteralPath $built -Destination $installed -Force
if((Get-FileHash -LiteralPath $installed).Hash -ne $release.Expected) { throw 'Installed DLL mismatch.' }
if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'Settings changed.' }
[pscustomobject]@{Installed=$installed;SHA256=$release.Expected;Backup=$backup;INIUnchanged=$true;InputChecks=$release.InputChecks;BodyArmChecks=$release.BodyArmChecks;InHeadsetTested=$false} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'deployment.json') -Encoding UTF8
Write-Output 'Installed the tested menu/R3 controls. DLL hash verified, previous DLL backed up, INI unchanged.'
