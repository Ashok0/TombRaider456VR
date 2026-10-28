$ErrorActionPreference = 'Stop'
$targetRoot = 'C:\dev\TombRaider123VR'
$stageRoot = $PSScriptRoot
$hashes = Get-Content -LiteralPath (Join-Path $stageRoot 'original-hashes.json') -Raw | ConvertFrom-Json
foreach ($entry in $hashes.PSObject.Properties) {
    $target = Join-Path $targetRoot $entry.Name
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $entry.Value) {
        throw "Target changed since staging: $($entry.Name)"
    }
}
$backupRoot = Join-Path $stageRoot ('before-port-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
foreach ($entry in $hashes.PSObject.Properties) {
    $target = Join-Path $targetRoot $entry.Name
    $staged = Join-Path $stageRoot $entry.Name
    if ((Get-FileHash -LiteralPath $staged -Algorithm SHA256).Hash -eq $entry.Value) { continue }
    $backup = Join-Path $backupRoot $entry.Name
    New-Item -ItemType Directory -Path (Split-Path $backup) -Force | Out-Null
    Copy-Item -LiteralPath $target -Destination $backup
    Copy-Item -LiteralPath $staged -Destination $target -Force
    Write-Output "Ported $($entry.Name)"
}
