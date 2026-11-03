$ErrorActionPreference='Stop'
if(Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Save and close Tomb Raider IV-VI before installation.' }
$gameDirectory='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$built=Join-Path $PSScriptRoot 'bin\TombRaiderVR.dll'
$installed=Join-Path $gameDirectory 'TombRaiderVR.dll'
$ini=Join-Path $gameDirectory 'TombRaiderVR.ini'
$iniBefore=(Get-FileHash -LiteralPath $ini -Algorithm SHA256).Hash
$expected=(Get-FileHash -LiteralPath $built -Algorithm SHA256).Hash
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$backupDirectory=Join-Path $PSScriptRoot "before-install-$stamp"
New-Item -ItemType Directory -Path $backupDirectory | Out-Null
$backup=Join-Path $backupDirectory 'TombRaiderVR.dll'
Copy-Item -LiteralPath $installed -Destination $backup
Copy-Item -LiteralPath $built -Destination $installed -Force
if((Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash -ne $expected) { throw 'Installed DLL hash differs from tested build.' }
if((Get-FileHash -LiteralPath $ini -Algorithm SHA256).Hash -ne $iniBefore) { throw 'INI changed unexpectedly.' }
[pscustomobject]@{Installed=$installed;Backup=$backup;SHA256=$expected;INIUnchanged=$true;FocusedChecks=3846;InHeadsetTested=$false;BaselineSuiteFailure='calibrated HMD height can request native crouch (also fails before this change)'} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot "deployment-$stamp.json") -Encoding UTF8
Write-Output 'Installed TR4/5 ledge-hang camera fix. Previous DLL backed up; installed hash verified; INI unchanged.'
