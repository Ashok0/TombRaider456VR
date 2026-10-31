$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$repo123='C:\dev\TombRaider123VR'
$stage=Join-Path $root 'build\switch-direct123'
$targets=@(
    @{repo=$repo123;game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered';process='tomb123';source=(Join-Path $stage 'bin\TombRaiderVR.dll');old='EB4C6FD0C2512B37DF3C2728DB6F0EC04C3AEEF0A03AA5F9E6C7F866007C31BF';new='9CDC69813950B315BF6DE3A92AC447DFA120F3CC1F3E532F2BE46AD06EDBC39E';checks=702},
    @{repo=$root;game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered';process='tomb456';source=(Join-Path $root 'build\switch-direct-release\TombRaiderVR.dll');old='A370D5B92C406A03E9EE83043E4E720AB3A85A7D28DEADC0D89C44A246905B17';new='FCC4E50CC0AEEEEEEAF43066F5A4534A737599057DE0068CD2CBB330C7224B48';checks=239}
)
foreach ($target in $targets) {
    $dll=Join-Path $target.game 'TombRaiderVR.dll'
    $installed=(Get-FileHash -LiteralPath $dll).Hash
    if ($installed -eq $target.new) { Write-Output "Already installed: $($target.process)";continue }
    if ($installed -ne $target.old) { throw "Installed DLL changed: $($target.game)" }
    if ((Get-FileHash -LiteralPath $target.source).Hash -ne $target.new) { throw 'Built DLL changed.' }
    $out=Join-Path $target.repo 'build\switch-direct'
    $backup=Join-Path $out ('before-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
    if ($target.repo -eq $repo123) {
        $baseline=Get-Content -LiteralPath (Join-Path $stage 'baseline.json') -Raw | ConvertFrom-Json
        $names=@($baseline.PSObject.Properties.Name)
        foreach ($name in $names) {
            $current=(Get-FileHash -LiteralPath (Join-Path $repo123 $name)).Hash
            if ($current -ne $baseline.$name -and $current -ne (Get-FileHash -LiteralPath (Join-Path $stage $name)).Hash) { throw "TR123 source changed: $name" }
        }
        foreach ($file in Get-ChildItem -LiteralPath (Join-Path $stage 'src') -File -Recurse) {
            $name=$file.FullName.Substring($stage.Length+1).Replace('\','/')
            if ($names -contains $name) {continue}
            if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $repo123 $name)).Hash) { throw "TR123 build input changed: $name" }
        }
        if ((Get-FileHash -LiteralPath (Join-Path $stage 'TombRaiderVR.vcxproj')).Hash -ne (Get-FileHash -LiteralPath (Join-Path $repo123 'TombRaiderVR.vcxproj')).Hash) { throw 'TR123 project changed.' }
        New-Item -ItemType Directory -Path $backup -Force | Out-Null
        foreach ($name in $names) {
            $saved=Join-Path $backup $name
            New-Item -ItemType Directory -Path (Split-Path -Parent $saved) -Force | Out-Null
            Copy-Item -LiteralPath (Join-Path $repo123 $name) -Destination $saved
            Copy-Item -LiteralPath (Join-Path $stage $name) -Destination (Join-Path $repo123 $name) -Force
            if ((Get-FileHash -LiteralPath (Join-Path $stage $name)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $repo123 $name)).Hash) { throw "Source copy failed: $name" }
        }
        Copy-Item -LiteralPath (Join-Path $stage 'bin') -Destination $out -Recurse -Force
    }
    if (Get-Process $target.process -ErrorAction SilentlyContinue) {
        Write-Output "PENDING: close $($target.process) to install $($target.source)"
        continue
    }
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    $ini=Join-Path $target.game 'TombRaiderVR.ini'
    $iniHash=(Get-FileHash -LiteralPath $ini).Hash
    Copy-Item -LiteralPath $dll,$ini -Destination $backup
    Copy-Item -LiteralPath $target.source -Destination $dll -Force
    if ((Get-FileHash -LiteralPath $dll).Hash -ne $target.new) { throw 'DLL verification failed.' }
    if ((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI changed.' }
    $record=@{deployed=(Get-Date -Format o);backup=$backup;game=$target.game;dll=$target.new;ini_unchanged=$iniHash;focused_checks=$target.checks;full_suite_run=$false;headset_tested=$false}
    $record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'deployment.json') -Encoding UTF8
    $record | ConvertTo-Json
}
