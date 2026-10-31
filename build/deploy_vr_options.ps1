$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$repo123='C:\dev\TombRaider123VR'
$stage=Join-Path $root 'build\vr-options123'
$newFiles=@('src/VRMenu.h','src/VRMenu.cpp','src/VRMenuModel.h','src/VRMenuSettings.h','src/VRMenuSettings.inl','tools/vr_menu_tests.cpp','build/test_vr_options.cmd')
$targets=@(
    @{repo=$root;game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered';process='tomb456';source=(Join-Path $root 'build\vr-options-release\TombRaiderVR.dll');old='225BAF3BD7D76278F32ED7EEF2BF6343221966B21414B4203D3DF5D27BCB99CE';new='E0F264E80E087EDFC4EADB9F500A97D042A77750965A81FF386DD75F439EC15A'},
    @{repo=$repo123;game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered';process='tomb123';source=(Join-Path $stage 'bin\TombRaiderVR.dll');old='3AF1A52DF2D0F9E74C15A31A3201D6EA510351782EC1BB97515BE434739A6627';new='F6196FE57CCC064AC7302B701AEDA21490E783D937B11C3E3D6D7B4DBB6C7CAC'}
)
foreach($target in $targets) {
    if(Get-Process $target.process -ErrorAction SilentlyContinue) { throw "Close $($target.process) before installing VR OPTIONS." }
    if((Get-FileHash -LiteralPath $target.source).Hash -ne $target.new) { throw 'Built DLL changed.' }
    if((Get-FileHash -LiteralPath (Join-Path $target.game 'TombRaiderVR.dll')).Hash -ne $target.old) { throw "Installed DLL changed: $($target.process)" }
}
$baseline=Get-Content -LiteralPath (Join-Path $stage 'baseline.json') -Raw | ConvertFrom-Json
$names=@($baseline.PSObject.Properties.Name)
foreach($name in $names) {
    if((Get-FileHash -LiteralPath (Join-Path $repo123 $name)).Hash -ne $baseline.$name) { throw "TR123 source changed during build: $name" }
}
foreach($name in $newFiles) { if(Test-Path -LiteralPath (Join-Path $repo123 $name)) { throw "New port file already exists: $name" } }
foreach($file in Get-ChildItem -LiteralPath (Join-Path $stage 'src') -File -Recurse) {
    $name=$file.FullName.Substring($stage.Length+1).Replace('\','/')
    if($names -contains $name -or $newFiles -contains $name) { continue }
    if((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $repo123 $name)).Hash) { throw "TR123 build input changed: $name" }
}
foreach($target in $targets) {
    $output=Join-Path $target.repo 'build\vr-options'
    $backup=Join-Path $output ('before-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
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
        Copy-Item -LiteralPath (Join-Path $stage 'bin') -Destination $output -Recurse -Force
    }
    Copy-Item -LiteralPath $target.source -Destination $dll -Force
    if((Get-FileHash -LiteralPath $dll).Hash -ne $target.new) { throw 'DLL verification failed.' }
    if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI unexpectedly changed.' }
    $record=@{deployed=(Get-Date -Format o);backup=$backup;dll=$target.new;ini_unchanged=$iniHash;controller_menu_checks=61;full_suite_run=$false;headset_tested=$false}
    $record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'deployment.json') -Encoding UTF8
    $record | ConvertTo-Json
}
