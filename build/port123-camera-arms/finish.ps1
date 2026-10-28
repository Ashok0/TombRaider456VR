$ErrorActionPreference = 'Stop'
$targetRoot = 'C:\dev\TombRaider123VR'
$hashes = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'original-hashes.json') -Raw | ConvertFrom-Json
foreach ($name in @('README.md','docs/collection-parity.md')) {
    $target = Join-Path $targetRoot $name
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $hashes.$name) { throw "Documentation changed: $name" }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $target
}
$artifactRoot = Join-Path $targetRoot 'build\camera-arm-port'
foreach ($name in @('locomotion.log','body.log','core.log','release.log','gait-before.log','crate-before.log')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $artifactRoot $name)
}
$sourceHashes = [ordered]@{}
foreach ($name in @('src/FirstPerson.cpp','src/BodyFollower.h','src/Config.h','src/DefaultIni.h','tests/collection_integration_selftest.h','tests/camera_clearance_selftest.h')) {
    $sourceHashes[$name]=(Get-FileHash -LiteralPath (Join-Path $targetRoot $name) -Algorithm SHA256).Hash
}
$manifest = [ordered]@{
    status='built-and-tested'
    change='Port crate base-camera clearance and sidestep/backpedal VRIK arm stability from TR4/5 to TR1/2/3.'
    created_at=(Get-Date).ToString('o')
    dll_sha256=(Get-FileHash -LiteralPath (Join-Path $artifactRoot 'release\TombRaiderVR.dll') -Algorithm SHA256).Hash
    source_sha256=$sourceHashes
    validation=@('Release x64 build','1144618 production locomotion/IK/camera checks','15654 body-parity checks','Core self-tests: zero failures','Both new regressions fail with their correction disabled')
    deployed_to_game=$false
    hardware_verified=$false
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $artifactRoot 'manifest.json') -Encoding UTF8
Write-Output 'README, port notes, validation logs and build manifest updated.'
