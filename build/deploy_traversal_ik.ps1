$ErrorActionPreference = 'Stop'
$repo = 'C:\dev\TombRaider456VR'
$game = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$release = Join-Path $repo 'build\traversal-ik-release'
if (Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Quit TR IV-VI before deploying.' }
$dll = Join-Path $game 'TombRaiderVR.dll'
$ini = Join-Path $game 'TombRaiderVR.ini'
if ((Get-FileHash -LiteralPath $dll).Hash -ne '5E690EE0C05E5CCD22429FA85BA1FD62965BAF700F26D11758A6E3A830CCD5AA') { throw 'Installed DLL changed; inspect before deploying.' }
$iniHash = (Get-FileHash -LiteralPath $ini).Hash
$backup = Join-Path $repo ('build\before-traversal-ik-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $dll,$ini -Destination $backup
$source = Join-Path $release 'TombRaiderVR.dll'
Copy-Item -LiteralPath $source -Destination $dll -Force
$hash = (Get-FileHash -LiteralPath $dll).Hash
if ($hash -ne (Get-FileHash -LiteralPath $source).Hash) { throw 'DLL verification failed.' }
if ($iniHash -ne (Get-FileHash -LiteralPath $ini).Hash) { throw 'INI unexpectedly changed.' }
$record = @{backup=$backup;deployed=(Get-Date -Format o);dll=$hash;ini_unchanged=$iniHash;standalone_checks=34315;headset_tested=$false}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $release 'deployment.json') -Encoding UTF8
$record | ConvertTo-Json
