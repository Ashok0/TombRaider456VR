$ErrorActionPreference='Stop'
$repo='C:\dev\TombRaider123VR'
$game='C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered'
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'source-manifest.json') -Raw | ConvertFrom-Json
$built=Join-Path $PSScriptRoot 'bin\TombRaiderVR.dll'
$expected='D9D02A7EB14FFD4C771338F9C70859B93121DCD9EBD32CBD40BF57969F5EF184'
$previous='B217D92D21CA7F4E2ED0E35B9F866CC3728241C465D252CA9CC2DC1497E746CE'
$installed=Join-Path $game 'TombRaiderVR.dll'
$ini=Join-Path $game 'TombRaiderVR.ini'
if(Get-Process tomb123 -ErrorAction SilentlyContinue) { throw 'Close TR1-3 before replacing its DLL.' }
if((Get-FileHash -LiteralPath $built -Algorithm SHA256).Hash -ne $expected) { throw 'Tested build changed.' }
if((Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash -ne $previous) { throw 'Installed DLL changed since inspection.' }
foreach($entry in $manifest) {
    $target=Join-Path $repo $entry.Relative
    $staged=Join-Path (Join-Path $PSScriptRoot 'work') $entry.Relative
    if((Get-FileHash -LiteralPath $staged -Algorithm SHA256).Hash -ne $entry.ModifiedHash) { throw "Staged source changed: $target" }
    if($entry.OriginalHash) {
        if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $entry.OriginalHash) { throw "Source changed since inspection: $target" }
    } elseif(Test-Path -LiteralPath $target) { throw "New file now exists: $target" }
}
$iniHash=(Get-FileHash -LiteralPath $ini -Algorithm SHA256).Hash
$backup=Join-Path $PSScriptRoot ('before-install-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $installed -Destination (Join-Path $backup 'TombRaiderVR.dll')
Copy-Item -LiteralPath $ini -Destination (Join-Path $backup 'TombRaiderVR.ini')
foreach($entry in $manifest) {
    if($entry.OriginalHash) {
        $save=Join-Path (Join-Path $backup 'source') $entry.Relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $save) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $repo $entry.Relative) -Destination $save
    }
}
foreach($entry in $manifest) {
    Copy-Item -LiteralPath (Join-Path (Join-Path $PSScriptRoot 'work') $entry.Relative) -Destination (Join-Path $repo $entry.Relative) -Force
}
$release=Join-Path $repo 'build\upjump-arm-continuity\bin'
New-Item -ItemType Directory -Path $release -Force | Out-Null
Copy-Item -LiteralPath $built -Destination (Join-Path $release 'TombRaiderVR.dll') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'bin\TombRaiderVR.pdb') -Destination $release -Force
Copy-Item -LiteralPath $built -Destination $installed -Force
if((Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash -ne $expected) { throw 'Installed DLL hash mismatch.' }
if((Get-FileHash -LiteralPath $ini -Algorithm SHA256).Hash -ne $iniHash) { throw 'INI changed.' }
foreach($entry in $manifest) {
    if((Get-FileHash -LiteralPath (Join-Path $repo $entry.Relative) -Algorithm SHA256).Hash -ne $entry.ModifiedHash) { throw 'Installed source hash mismatch.' }
}
[pscustomobject]@{
    Installed=$installed;Backup=$backup;SHA256=$expected;INIUnchanged=$true
    FocusedChecks=16198;FullRegressionChecks=1234382;PhysicsGraphicsChecks=2314;InHeadsetTested=$false
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'deployment.json') -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'deployment.json') -Destination (Join-Path $repo 'build\upjump-arm-continuity\deployment.json')
Write-Output 'Ported source to TR1-3 and installed the tested arm continuity DLL. Backup saved; hashes verified; INI unchanged.'
