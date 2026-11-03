$ErrorActionPreference='Stop'
$hybridRoot=$PSScriptRoot
if (Get-Process | Where-Object { $_.ProcessName -match '^tomb|TombRaider' }) {
    throw 'Close Tomb Raider before installing these builds.'
}
$manifest=Get-Content -LiteralPath "$hybridRoot\deployment-manifest.json" -Raw | ConvertFrom-Json
foreach ($entry in $manifest) {
    $ini=Join-Path $entry.directory 'TombRaiderVR.ini'
    if ((Get-FileHash -LiteralPath $ini -Algorithm SHA256).Hash -ne $entry.ini_before_sha256) {
        throw "INI changed since preparation: $ini"
    }
    $built=Join-Path "$hybridRoot\$($entry.collection)\bin" 'TombRaiderVR.dll'
    if ((Get-FileHash -LiteralPath $built -Algorithm SHA256).Hash -ne $entry.dll_sha256) { throw 'Build hash mismatch' }
}
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$records=@()
foreach ($entry in $manifest) {
    $collection=$entry.collection
    foreach ($relative in @('TombRaiderVR.dll','TombRaiderVR.ini','TombRaiderVRBindings\actions.json','TombRaiderVRBindings\bindings_index.json')) {
        $source=if ($relative -eq 'TombRaiderVR.ini') { Join-Path "$hybridRoot\$collection" $relative } else { Join-Path "$hybridRoot\$collection\bin" $relative }
        $target=Join-Path $entry.directory $relative
        $backup=Join-Path "$hybridRoot\before-deploy-$stamp\$collection" $relative
        New-Item -ItemType Directory -Path (Split-Path $backup) -Force | Out-Null
        if (Test-Path -LiteralPath $target) { Copy-Item -LiteralPath $target -Destination $backup }
        New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $target -Force
        $expected=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $expected) { throw "Installed hash mismatch: $target" }
        $records += [pscustomobject]@{ Collection=$collection;Installed=$target;Backup=$backup;SHA256=$expected }
    }
}
$records | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$hybridRoot\deployment.json" -Encoding UTF8
Write-Output 'Both mods installed and hash-verified. HybridModernTankControls=0 in both installed INIs. Backups retained.'
