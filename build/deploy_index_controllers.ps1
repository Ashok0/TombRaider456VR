$ErrorActionPreference = 'Stop'
$repo = 'C:\dev\TombRaider456VR'
$game = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'
$release = Join-Path $repo 'build\index-controllers-release'
if (Get-Process tomb456 -ErrorAction SilentlyContinue) { throw 'Quit TR IV-VI before deploying.' }
$dll = Join-Path $game 'TombRaiderVR.dll'
$ini = Join-Path $game 'TombRaiderVR.ini'
if ((Get-FileHash -LiteralPath $dll).Hash -ne 'EEC6E0B904108424450579C6CBEB77BFB4CB3149D619413354AEABF1AA9079E8') { throw 'Installed DLL changed; inspect before deploying.' }
if ((Get-FileHash -LiteralPath $ini).Hash -ne 'FF8F10E3111D5C6B5B681C0AC947B6B09EDF0F66BB061432AD03EF01FFFF7CB4') { throw 'Installed INI changed; inspect before deploying.' }
$backup = Join-Path $repo ('build\before-index-controllers-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $dll,$ini -Destination $backup
$bindings = Join-Path $game 'TombRaiderVRBindings'
if (Test-Path -LiteralPath $bindings) { Copy-Item -LiteralPath $bindings -Destination $backup -Recurse }
$text = [IO.File]::ReadAllText($ini)
if ($text -match '(?im)^ValveIndexControllers=') { throw 'Index settings unexpectedly already present.' }
$section = [regex]::Match($text,'(?m)^\[VR\]\r*\n')
if (!$section.Success) { throw 'Missing VR INI section.' }
$addition = "; Index controller profile: set to 1 and restart when using Index.`r`nValveIndexControllers=0`r`nValveIndexGripPress=0.55`r`nValveIndexGripRelease=0.35`r`n"
$updated = $text.Insert($section.Index + $section.Length,$addition)
New-Item -ItemType Directory -Path $bindings -Force | Out-Null
$files = @('TombRaiderVR.dll','TombRaiderVRBindings\actions.json','TombRaiderVRBindings\bindings_index.json')
foreach ($relative in $files) {
    $from = Join-Path $release $relative
    $to = Join-Path $game $relative
    Copy-Item -LiteralPath $from -Destination $to -Force
    if ((Get-FileHash -LiteralPath $from).Hash -ne (Get-FileHash -LiteralPath $to).Hash) { throw "Copy verification failed: $relative" }
}
[IO.File]::WriteAllText($ini,$updated,[Text.UTF8Encoding]::new($false))
if ([IO.File]::ReadAllText($ini).Replace($addition,'') -cne $text) { throw 'INI preservation verification failed.' }
$hashes = @{}
foreach ($relative in ($files + 'TombRaiderVR.ini')) { $hashes[$relative]=(Get-FileHash -LiteralPath (Join-Path $game $relative)).Hash }
$record = @{backup=$backup;deployed=(Get-Date -Format o);hashes=$hashes;index_mode_enabled=$false;standalone_checks=306;hardware_tested=$false}
$record | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $release 'deployment.json') -Encoding UTF8
$record | ConvertTo-Json -Depth 4
