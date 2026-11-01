$ErrorActionPreference='Stop'
$stage='C:\dev\TombRaider456VR\build\special123-port'
$target='C:\dev\TombRaider123VR'
$manifest=Get-Content -Raw -LiteralPath (Join-Path $stage 'manifest.json') | ConvertFrom-Json
foreach($row in $manifest) {
    $source=Join-Path $stage $row.path
    $destination=Join-Path $target $row.path
    if((Get-FileHash -LiteralPath $source).Hash -ne $row.new) { throw "Staged file changed: $source" }
    if($row.old) {
        if((Get-FileHash -LiteralPath $destination).Hash -ne $row.old) { throw "Source file changed: $destination" }
    } elseif(Test-Path -LiteralPath $destination) { throw "New file already exists: $destination" }
}
$backup=Join-Path $target ('build\special-switch\before-source-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
foreach($row in $manifest) {
    $source=Join-Path $stage $row.path
    $destination=Join-Path $target $row.path
    if($row.old) {
        $save=Join-Path $backup $row.path
        New-Item -ItemType Directory -Force -Path (Split-Path $save) | Out-Null
        Copy-Item -LiteralPath $destination -Destination $save
    }
    Copy-Item -LiteralPath $source -Destination $destination
}
Write-Output "Applied $($manifest.Count) files; source backup: $backup"
