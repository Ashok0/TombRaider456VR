$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$release=Join-Path $root 'build\floor-lever-release'
if (Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Close TR4/5 before installing the floor-lever build.' }
$dll=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$source=Join-Path $release 'TombRaiderVR.dll'
$expected='B38BE5515620F28228488B77381F167C778F2B0BB69E81BD053DFA0DB8175884'
if ((Get-FileHash -LiteralPath $dll).Hash -ne 'FCC4E50CC0AEEEEEEAF43066F5A4534A737599057DE0068CD2CBB330C7224B48') { throw 'Installed DLL changed; inspect before replacing.' }
if ((Get-FileHash -LiteralPath $source).Hash -ne $expected) { throw 'Built DLL changed.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\before-floor-lever-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $dll,$ini -Destination $backup
Copy-Item -LiteralPath $source -Destination $dll -Force
if ((Get-FileHash -LiteralPath $dll).Hash -ne $expected) { throw 'DLL verification failed.' }
if ((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI unexpectedly changed.' }
$record=@{deployed=(Get-Date -Format o);backup=$backup;dll=$expected;ini_unchanged=$iniHash;focused_checks=1381;asset_paths=60;floor_asset_paths=22;train_asset_paths=2;full_suite_run=$false;headset_tested=$false}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $release 'deployment.json') -Encoding UTF8
$record | ConvertTo-Json
