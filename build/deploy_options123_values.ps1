$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$repo123='C:\dev\TombRaider123VR'
$stage=Join-Path $root 'build\vr-options123'
$newFiles=@()
$targets=@(
    @{repo=$repo123;game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered';process='tomb123';source=(Join-Path $stage 'bin\TombRaiderVR.dll');old='6a3cbb6575303086ab633f97ebe21ed01a978eac339fde0b68d3d1bb5834b200';new='2a33d170071a7989ac696c9de547f1a1260b9c7403c19fcc2ecc46aa2d37fb43'}
)

foreach($target in $targets) {
    if(Get-Process $target.process -ErrorAction SilentlyContinue) { throw "Close $($target.process) before installing native VR Options." }
    if((Get-FileHash -LiteralPath $target.source).Hash -ne $target.new) { throw 'Built DLL changed.' }
    if((Get-FileHash -LiteralPath (Join-Path $target.game 'TombRaiderVR.dll')).Hash -ne $target.old) { throw "Installed DLL changed: $($target.process)" }
}
$baseline=Get-Content -LiteralPath (Join-Path $root 'build/options123-values/baseline123.json') -Raw | ConvertFrom-Json
$names=@($baseline.PSObject.Properties.Name)
foreach($name in $names) {
    if((Get-FileHash -LiteralPath (Join-Path $repo123 $name)).Hash -ne $baseline.$name) { throw "TR123 source changed: $name" }
}
$sources=Get-Content -LiteralPath (Join-Path $root 'build/options123-values/sources123.json') -Raw | ConvertFrom-Json
foreach($name in $sources.PSObject.Properties.Name) {
    if((Get-FileHash -LiteralPath (Join-Path $repo123 $name)).Hash -ne $sources.$name) { throw "TR123 build input changed: $name" }
}
foreach($name in $newFiles) { if(Test-Path -LiteralPath (Join-Path $repo123 $name)) { throw "New file already exists: $name" } }
foreach($target in $targets) {
    $output=Join-Path $target.repo 'build\vr-options'
    $backup=Join-Path $output ('before-visible-options-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    $dll=Join-Path $target.game 'TombRaiderVR.dll'
    $ini=Join-Path $target.game 'TombRaiderVR.ini'
    $iniHash=(Get-FileHash -LiteralPath $ini).Hash
    Copy-Item -LiteralPath $dll,$ini -Destination $backup
    if($target.repo -eq $repo123) {
        foreach($name in @($names)+@($newFiles)) {
            $destination=Join-Path $repo123 $name
            if($names -contains $name) {
                $saved=Join-Path $backup $name
                New-Item -ItemType Directory -Path (Split-Path -Parent $saved) -Force | Out-Null
                Copy-Item -LiteralPath $destination -Destination $saved
            }
            New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
            Copy-Item -LiteralPath (Join-Path $stage $name) -Destination $destination -Force
            if((Get-FileHash -LiteralPath $destination).Hash -ne (Get-FileHash -LiteralPath (Join-Path $stage $name)).Hash) { throw "Source copy failed: $name" }
        }
        $bin=Join-Path $output 'bin'
        New-Item -ItemType Directory -Path $bin -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $stage 'bin\TombRaiderVR.dll'),(Join-Path $stage 'bin\TombRaiderVR.pdb') -Destination $bin -Force
    }
    Copy-Item -LiteralPath $target.source -Destination $dll -Force
    if((Get-FileHash -LiteralPath $dll).Hash -ne $target.new) { throw 'DLL verification failed.' }
    if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI unexpectedly changed.' }
    $record=@{deployed=(Get-Date -Format o);backup=$backup;dll=$target.new;ini_unchanged=$iniHash;native_options_checks=109;settings_checks=55;verified_game_layouts=9;change='Plain-text Controller Type and Color Scheme values; avoid native icon token parsing';full_suite_run=$false;headset_tested=$false}
    $record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'visible-options-deployment.json') -Encoding UTF8
    $record | ConvertTo-Json
}
