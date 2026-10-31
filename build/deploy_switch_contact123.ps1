$ErrorActionPreference='Stop'
$repo='C:\dev\TombRaider123VR'
$stage='C:\dev\TombRaider456VR\build\switch-contact123'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered'
$expected='3AF1A52DF2D0F9E74C15A31A3201D6EA510351782EC1BB97515BE434739A6627'
$previous='9CDC69813950B315BF6DE3A92AC447DFA120F3CC1F3E532F2BE46AD06EDBC39E'
$source=Join-Path $stage 'bin\TombRaiderVR.dll'
$installed=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
if (Get-Process tomb123 -ErrorAction SilentlyContinue) { throw 'Close TR1-3 before installing the switch contact port.' }
if ((Get-FileHash -LiteralPath $source).Hash -ne $expected) { throw 'Built DLL changed.' }
if ((Get-FileHash -LiteralPath $installed).Hash -ne $previous) { throw 'Installed DLL changed.' }
$baseline=Get-Content -LiteralPath (Join-Path $stage 'baseline.json') -Raw | ConvertFrom-Json
$names=@($baseline.PSObject.Properties.Name)
foreach($name in $names) {
    if ((Get-FileHash -LiteralPath (Join-Path $repo $name)).Hash -ne $baseline.$name) { throw "Source changed during build: $name" }
}
foreach($file in Get-ChildItem -LiteralPath (Join-Path $stage 'src') -File -Recurse) {
    $name=$file.FullName.Substring($stage.Length+1).Replace('\','/')
    if ($names -contains $name) { continue }
    if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $repo $name)).Hash) { throw "Build input changed: $name" }
}
if ((Get-FileHash -LiteralPath (Join-Path $stage 'TombRaiderVR.vcxproj')).Hash -ne (Get-FileHash -LiteralPath (Join-Path $repo 'TombRaiderVR.vcxproj')).Hash) { throw 'Project changed.' }
$iniHash=(Get-FileHash -LiteralPath $ini).Hash
$output=Join-Path $repo 'build\switch-contact'
$backup=Join-Path $output ('before-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath $installed,$ini -Destination $backup
foreach($name in $names) {
    $saved=Join-Path $backup $name
    New-Item -ItemType Directory -Path (Split-Path -Parent $saved) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repo $name) -Destination $saved
    Copy-Item -LiteralPath (Join-Path $stage $name) -Destination (Join-Path $repo $name) -Force
    if ((Get-FileHash -LiteralPath (Join-Path $repo $name)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $stage $name)).Hash) { throw "Source copy failed: $name" }
}
Copy-Item -LiteralPath (Join-Path $stage 'bin') -Destination $output -Recurse -Force
Copy-Item -LiteralPath $source -Destination $installed -Force
if ((Get-FileHash -LiteralPath $installed).Hash -ne $expected) { throw 'Installed DLL verification failed.' }
if ((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI unexpectedly changed.' }
$record=@{deployed=(Get-Date -Format o);backup=$backup;dll=$expected;ini_unchanged=$iniHash;focused_checks=848;asset_paths=204;full_suite_run=$false;headset_tested=$false}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'deployment.json') -Encoding UTF8
$record | ConvertTo-Json
