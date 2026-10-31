$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$release=Join-Path $root 'build\switch-contact-release'
if (Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Close TR4/5 before installing the switch-contact build.' }
$dll=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
$source=Join-Path $release 'TombRaiderVR.dll'
$expected='225BAF3BD7D76278F32ED7EEF2BF6343221966B21414B4203D3DF5D27BCB99CE'
if ((Get-FileHash -LiteralPath $dll).Hash -ne '8786B0968D105F12D37BBB3D8A546A6998F8093DD3989C9DB4DF795480F0ABEA') { throw 'Installed DLL changed; inspect before replacing.' }
if ((Get-FileHash -LiteralPath $source).Hash -ne $expected) { throw 'Built DLL changed.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$backup=Join-Path $root ('build\before-switch-contact-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $dll,$ini -Destination $backup
Copy-Item -LiteralPath $source -Destination $dll -Force
if ((Get-FileHash -LiteralPath $dll).Hash -ne $expected) { throw 'DLL verification failed.' }
if ((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI unexpectedly changed.' }
$record=@{deployed=(Get-Date -Format o);backup=$backup;dll=$expected;ini_unchanged=$iniHash;focused_checks=2347;contact_mesh_checks=11;full_suite_run=$false;headset_tested=$false}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $release 'deployment.json') -Encoding UTF8
$record | ConvertTo-Json

