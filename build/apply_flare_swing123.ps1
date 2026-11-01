$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider123VR'
$stage='C:\dev\TombRaider456VR\build\flare-swing123-port'
$hashes=Get-Content -LiteralPath (Join-Path $stage 'original-hashes.json') -Raw | ConvertFrom-Json
foreach($p in $hashes.PSObject.Properties) {
    if((Get-FileHash -LiteralPath (Join-Path $root $p.Name)).Hash -ne $p.Value) {throw "Source changed: $($p.Name)"}
}
$backup=Join-Path $root ('build\physical-flare\before-swing-source-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
foreach($p in $hashes.PSObject.Properties) {
    $save=Join-Path $backup $p.Name
    New-Item -ItemType Directory -Path (Split-Path -Parent $save) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $p.Name) -Destination $save
}
foreach($p in $hashes.PSObject.Properties) {
    Copy-Item -LiteralPath (Join-Path $stage $p.Name) -Destination (Join-Path $root $p.Name) -Force
}
Write-Output "Applied automatic flare swing port; previous source backed up in $backup"
