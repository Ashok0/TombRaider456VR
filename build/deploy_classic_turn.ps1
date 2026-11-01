$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$repo123='C:\dev\TombRaider123VR'
$stage=Join-Path $root 'build\vr-options123'
$newFiles=@('src/ClassicBodyRender.h','src/ClassicBodyAddresses.inl','tools/classic_body_tests.cpp','build/test_classic_body.cmd')
$targets=@(
    @{repo=$root;game='C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered';process='tomb456';source='C:/dev/TombRaider456VR/build/vr-options-release/TombRaiderVR.dll';old='e9d71f4a120d6b0bb7c6be3307b228e329cf43502b5cf8f862fd5d029938042b';new='5b784d536095f2803c28dd50b8af32407d582035f755f4c51e3e606b44085e82'},
    @{repo=$repo123;game='C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider I-III Remastered';process='tomb123';source='C:/dev/TombRaider456VR/build/vr-options123/bin/TombRaiderVR.dll';old='2a33d170071a7989ac696c9de547f1a1260b9c7403c19fcc2ecc46aa2d37fb43';new='e673233f411a41719b222590358a5b888faea1a0272c58850b2402e0961eaab7'}
)

foreach($target in $targets) {
    if(Get-Process $target.process -ErrorAction SilentlyContinue) { throw "Close $($target.process) before installing native VR Options." }
    if((Get-FileHash -LiteralPath $target.source).Hash -ne $target.new) { throw 'Built DLL changed.' }
    if((Get-FileHash -LiteralPath (Join-Path $target.game 'TombRaiderVR.dll')).Hash -ne $target.old) { throw "Installed DLL changed: $($target.process)" }
}
$baseline=Get-Content -LiteralPath (Join-Path $root 'build/classic-turn/baseline123.json') -Raw | ConvertFrom-Json
$names=@($baseline.PSObject.Properties.Name)
foreach($name in $names) {
    if((Get-FileHash -LiteralPath (Join-Path $repo123 $name)).Hash -ne $baseline.$name) { throw "TR123 source changed: $name" }
}
$sources=Get-Content -LiteralPath (Join-Path $root 'build/classic-turn/sources123.json') -Raw | ConvertFrom-Json
foreach($name in $sources.PSObject.Properties.Name) {
    if((Get-FileHash -LiteralPath (Join-Path $repo123 $name)).Hash -ne $sources.$name) { throw "TR123 build input changed: $name" }
}
foreach($name in $newFiles) { if(Test-Path -LiteralPath (Join-Path $repo123 $name)) { throw "New file already exists: $name" } }
foreach($target in $targets) {
    $output=Join-Path $target.repo 'build\vr-options'
    $backup=Join-Path $output ('before-classic-turn-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
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
    $record=@{deployed=(Get-Date -Format o);backup=$backup;dll=$target.new;ini_unchanged=$iniHash;classic_body_checks=407;menu_regression_checks=164;verified_game_layouts=13;change='Classic first-person body centering and live physical-turn fallback in TR1-5';full_suite_run=$false;headset_tested=$false}
    $record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'classic-turn-deployment.json') -Encoding UTF8
    $record | ConvertTo-Json
}
