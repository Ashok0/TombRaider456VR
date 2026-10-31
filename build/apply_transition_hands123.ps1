$ErrorActionPreference = 'Stop'
$stage = 'C:\dev\TombRaider456VR\build\transition-hands123'
$repo = 'C:\dev\TombRaider123VR'
$game = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered'
$baseline = Get-Content -LiteralPath (Join-Path $stage 'baseline.json') -Raw | ConvertFrom-Json
$existing = @($baseline.PSObject.Properties.Name)
$added = @('tests/transition_hands_selftest.cpp','tests/build_transition_hands.cmd')
foreach ($name in $existing) {
    if ((Get-FileHash -LiteralPath (Join-Path $repo $name)).Hash -ne $baseline.$name) { throw "Source changed since staging: $name" }
}
foreach ($name in $added) {
    if (Test-Path -LiteralPath (Join-Path $repo $name)) { throw "New target already exists: $name" }
}
# Check all other production sources used by this build to avoid publishing a
# stale snapshot over independent changes in the target repository.
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $stage 'src') -File -Recurse) {
    $name = $file.FullName.Substring($stage.Length+1).Replace('\','/')
    if ($existing -contains $name) { continue }
    if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $repo $name)).Hash) { throw "Build input changed: $name" }
}
if ((Get-FileHash -LiteralPath (Join-Path $stage 'TombRaiderVR.vcxproj')).Hash -ne (Get-FileHash -LiteralPath (Join-Path $repo 'TombRaiderVR.vcxproj')).Hash) { throw 'Project build inputs changed.' }
if (Get-Process tomb123 -ErrorAction SilentlyContinue) { throw 'Quit TR I-III before deploying.' }
$installed = Join-Path $game 'TombRaiderVR.dll'
if ((Get-FileHash -LiteralPath $installed).Hash -ne 'E8232F08D9750B2F09DE94B90A4D4DEF0307607AFFF60D81FD8072721C3B68E5') { throw 'Installed TR123 DLL changed.' }
$ini = Join-Path $game 'TombRaiderVR.ini'
$iniHash = (Get-FileHash -LiteralPath $ini).Hash
$sourceDll = Join-Path $stage 'bin\TombRaiderVR.dll'
if ((Get-FileHash -LiteralPath $sourceDll).Hash -ne '68E1553632D1FF5AC2945BA5F58353E348345AFEF4AF0646175BC8973B8BAFAE') { throw 'Staged DLL changed.' }
$out = Join-Path $repo 'build\transition-hands-port'
$backup = Join-Path $out ('before-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
foreach ($name in $existing) {
    $saved = Join-Path $backup $name
    New-Item -ItemType Directory -Path (Split-Path -Parent $saved) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repo $name) -Destination $saved
}
Copy-Item -LiteralPath $installed -Destination (Join-Path $backup 'installed-TombRaiderVR.dll')
Copy-Item -LiteralPath $ini -Destination (Join-Path $backup 'installed-TombRaiderVR.ini')
$hashes = @{}
foreach ($name in ($existing+$added)) {
    Copy-Item -LiteralPath (Join-Path $stage $name) -Destination (Join-Path $repo $name) -Force
    $hashes[$name]=(Get-FileHash -LiteralPath (Join-Path $repo $name)).Hash
    if ($hashes[$name] -ne (Get-FileHash -LiteralPath (Join-Path $stage $name)).Hash) { throw "Source verification failed: $name" }
}
Copy-Item -LiteralPath (Join-Path $stage 'bin') -Destination $out -Recurse -Force
Copy-Item -LiteralPath $sourceDll -Destination $installed -Force
$dllHash=(Get-FileHash -LiteralPath $installed).Hash
if ($dllHash -ne (Get-FileHash -LiteralPath $sourceDll).Hash) { throw 'Installed DLL verification failed.' }
if ((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'Installed INI unexpectedly changed.' }
$record=@{backup=$backup;deployed=(Get-Date -Format o);dll=$dllHash;ini_unchanged=$iniHash;files=$hashes;focused_checks=2864;full_suite_run=$false;headset_tested=$false}
$record | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'deployment.json') -Encoding UTF8
$record | ConvertTo-Json -Depth 4
