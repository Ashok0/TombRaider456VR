$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$repo123='C:\dev\TombRaider123VR'
$stage=Join-Path $root 'build\vr-options123'
$newFiles=@('src/VRNativeMenuExtra.inl')
$targets=@(
    @{repo=$root;game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered';process='tomb456';source=(Join-Path $root 'build\vr-options-release\TombRaiderVR.dll');old='8E5C7310648F7605238E788A4E1E94A8389532374496A3363DD54CD7485E3F5D';new='bed2ad811e7b388b4ec453eab7cf02dbb1c8b5af3c9e3b3cefc772732cbb40f9'},
    @{repo=$repo123;game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered';process='tomb123';source=(Join-Path $stage 'bin\TombRaiderVR.dll');old='267FDB8002E3FEA1BBBCD531E73ECB64A0D708A189715E0B01A81D4DDFC0CBC1';new='a5ac946e71f0616e40498efdae31131987379ccf3061552f135b842bdbce2061'}
)
foreach($target in $targets) {
    if(Get-Process $target.process -ErrorAction SilentlyContinue) { throw "Close $($target.process) before installing native VR Options." }
    if((Get-FileHash -LiteralPath $target.source).Hash -ne $target.new) { throw 'Built DLL changed.' }
    if((Get-FileHash -LiteralPath (Join-Path $target.game 'TombRaiderVR.dll')).Hash -ne $target.old) { throw "Installed DLL changed: $($target.process)" }
}
$baseline=Get-Content -LiteralPath (Join-Path $root 'build/controller-type-only/baseline123.json') -Raw | ConvertFrom-Json
$names=@($baseline.PSObject.Properties.Name)
foreach($name in $names) {
    if((Get-FileHash -LiteralPath (Join-Path $repo123 $name)).Hash -ne $baseline.$name) { throw "TR123 source changed: $name" }
}
$sources=Get-Content -LiteralPath (Join-Path $root 'build/controller-type-only/sources123.json') -Raw | ConvertFrom-Json
foreach($name in $sources.PSObject.Properties.Name) {
    if((Get-FileHash -LiteralPath (Join-Path $repo123 $name)).Hash -ne $sources.$name) { throw "TR123 build input changed: $name" }
}
foreach($name in $newFiles) { if(Test-Path -LiteralPath (Join-Path $repo123 $name)) { throw "New file already exists: $name" } }
foreach($target in $targets) {
    $output=Join-Path $target.repo 'build\vr-options'
    $backup=Join-Path $output ('before-controller-type-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
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
    $record=@{deployed=(Get-Date -Format o);backup=$backup;dll=$target.new;ini_unchanged=$iniHash;controller_menu_checks=43;native_structure_checks=21;verified_layouts=15;full_suite_run=$false;headset_tested=$false}
    $record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'controller-type-deployment.json') -Encoding UTF8
    $record | ConvertTo-Json
}
