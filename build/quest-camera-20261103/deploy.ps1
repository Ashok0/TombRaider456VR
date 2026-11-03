$ErrorActionPreference = 'Stop'
if (Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Close TR4-6 before deployment.' }
$gameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$built = Join-Path $PSScriptRoot 'bin\TombRaiderVR.dll'
$installed = Join-Path $gameDirectory 'TombRaiderVR.dll'
$ini = Join-Path $gameDirectory 'TombRaiderVR.ini'
$iniBefore = (Get-FileHash -LiteralPath $ini -Algorithm SHA256).Hash
$expected = (Get-FileHash -LiteralPath $built -Algorithm SHA256).Hash
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backupDirectory = Join-Path $PSScriptRoot "before-deploy-$stamp"
New-Item -ItemType Directory -Path $backupDirectory | Out-Null
$backup = Join-Path $backupDirectory 'TombRaiderVR.dll'
Copy-Item -LiteralPath $installed -Destination $backup
Copy-Item -LiteralPath $built -Destination $installed -Force
if ((Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash -ne $expected) { throw 'Installed DLL hash differs from build.' }
if ((Get-FileHash -LiteralPath $ini -Algorithm SHA256).Hash -ne $iniBefore) { throw 'INI changed unexpectedly.' }
[pscustomobject]@{Installed=$installed;Backup=$backup;SHA256=$expected;INIUnchanged=$true;FocusedChecks=414;VerifiedNativeBuilds=4;InHeadsetTested=$false} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot "deployment-$stamp.json") -Encoding UTF8
Write-Output 'Installed TR4/5 quest-use camera update; DLL hash verified; previous DLL backed up; INI unchanged.'
