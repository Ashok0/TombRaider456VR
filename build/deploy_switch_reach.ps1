$ErrorActionPreference = 'Stop'
$root = 'C:\dev\TombRaider456VR'
$repo123 = 'C:\dev\TombRaider123VR'
$stage = Join-Path $root 'build\switch-hand-reach123'
$baseline = Get-Content -LiteralPath (Join-Path $stage 'baseline.json') -Raw | ConvertFrom-Json
$existing = @($baseline.PSObject.Properties.Name)
$added = @('tests/switch_reach_selftest.cpp','tests/build_switch_reach.cmd')
foreach ($name in $existing) {
    if ((Get-FileHash -LiteralPath (Join-Path $repo123 $name)).Hash -ne $baseline.$name) { throw "TR123 source changed: $name" }
}
foreach ($name in $added) {
    if (Test-Path -LiteralPath (Join-Path $repo123 $name)) { throw "New target already exists: $name" }
}
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $stage 'src') -File -Recurse) {
    $name = $file.FullName.Substring($stage.Length+1).Replace('\','/')
    if ($existing -contains $name) { continue }
    if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $repo123 $name)).Hash) { throw "TR123 build input changed: $name" }
}
if ((Get-FileHash -LiteralPath (Join-Path $stage 'TombRaiderVR.vcxproj')).Hash -ne (Get-FileHash -LiteralPath (Join-Path $repo123 'TombRaiderVR.vcxproj')).Hash) { throw 'TR123 project changed.' }
$targets = @(
    @{ repo=$repo123; game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered'; process='tomb123'; source=(Join-Path $stage 'bin\TombRaiderVR.dll'); old='F79BDDBFC2166DB841B3BC2D291A4BF2E7B0DE4FE8DC16FD5C0EF8C653C83354'; new='EB4C6FD0C2512B37DF3C2728DB6F0EC04C3AEEF0A03AA5F9E6C7F866007C31BF'; checks=672 },
    @{ repo=$root; game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered'; process='tomb456'; source=(Join-Path $root 'build\switch-hand-reach-release\TombRaiderVR.dll'); old='F42B4C3933659ACB094C8783CAC9CEC4F145404984AE959D545709CE354EBFE7'; new='A370D5B92C406A03E9EE83043E4E720AB3A85A7D28DEADC0D89C44A246905B17'; checks=224 }
)
foreach ($target in $targets) {
    if (Get-Process $target.process -ErrorAction SilentlyContinue) { throw "Quit $($target.process) before deploying." }
    if ((Get-FileHash -LiteralPath (Join-Path $target.game 'TombRaiderVR.dll')).Hash -ne $target.old) { throw "Installed DLL changed: $($target.game)" }
    if ((Get-FileHash -LiteralPath $target.source).Hash -ne $target.new) { throw "Built DLL changed: $($target.source)" }
}
foreach ($target in $targets) {
    $out = Join-Path $target.repo 'build\switch-hand-reach'
    $backup = Join-Path $out ('before-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    $dll = Join-Path $target.game 'TombRaiderVR.dll'
    $ini = Join-Path $target.game 'TombRaiderVR.ini'
    $iniHash = (Get-FileHash -LiteralPath $ini).Hash
    Copy-Item -LiteralPath $dll,$ini -Destination $backup
    if ($target.repo -eq $repo123) {
        foreach ($name in $existing) {
            $saved = Join-Path $backup $name
            New-Item -ItemType Directory -Path (Split-Path -Parent $saved) -Force | Out-Null
            Copy-Item -LiteralPath (Join-Path $repo123 $name) -Destination $saved
        }
        foreach ($name in ($existing+$added)) {
            Copy-Item -LiteralPath (Join-Path $stage $name) -Destination (Join-Path $repo123 $name) -Force
            if ((Get-FileHash -LiteralPath (Join-Path $stage $name)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $repo123 $name)).Hash) { throw "Source copy failed: $name" }
        }
        Copy-Item -LiteralPath (Join-Path $stage 'bin') -Destination $out -Recurse -Force
    }
    Copy-Item -LiteralPath $target.source -Destination $dll -Force
    if ((Get-FileHash -LiteralPath $dll).Hash -ne $target.new) { throw 'Installed DLL verification failed.' }
    if ((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI unexpectedly changed.' }
    $record = @{deployed=(Get-Date -Format o);backup=$backup;game=$target.game;dll=$target.new;ini_unchanged=$iniHash;focused_checks=$target.checks;full_suite_run=$false;headset_tested=$false}
    $record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'deployment.json') -Encoding UTF8
    $record | ConvertTo-Json
}
