$ErrorActionPreference='Stop'
$failed='C:\dev\TombRaider456VR\build\hands-out-capture'
$repo123='C:\dev\TombRaider123VR'
$manifest=Get-Content -LiteralPath (Join-Path $failed 'source123-manifest.json') -Raw | ConvertFrom-Json
$releases=Get-Content -LiteralPath (Join-Path $failed 'releases.json') -Raw | ConvertFrom-Json
$deployment=Get-Content -LiteralPath (Join-Path $failed 'deployment.json') -Raw | ConvertFrom-Json
$sourceBackup=Join-Path (Split-Path -Parent $deployment[0].Backup) '123-source'
foreach($r in $releases) {
    if(Get-Process $r.Process -ErrorAction SilentlyContinue) { throw 'Game reopened; DLL is locked.' }
    $installed=Join-Path $r.Directory 'TombRaiderVR.dll'
    $saved=Join-Path ($deployment | Where-Object Collection -eq $r.Id).Backup 'TombRaiderVR.dll'
    if((Get-FileHash -LiteralPath $installed).Hash -ne $r.Expected) { throw 'Installed DLL changed.' }
    if((Get-FileHash -LiteralPath $saved).Hash -ne $r.Previous) { throw 'Prior DLL backup mismatch.' }
}
foreach($entry in $manifest) {
    $target=Join-Path $repo123 $entry.Relative
    if((Get-FileHash -LiteralPath $target).Hash -ne $entry.ModifiedHash) { throw 'TR1-3 source changed.' }
    if($entry.OriginalHash -and (Get-FileHash -LiteralPath (Join-Path $sourceBackup $entry.Relative)).Hash -ne $entry.OriginalHash) { throw 'Source backup mismatch.' }
}
$backup=Join-Path $PSScriptRoot ('before-rollback-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
foreach($entry in $manifest) {
    $target=Join-Path $repo123 $entry.Relative
    $save=Join-Path (Join-Path $backup '123-source') $entry.Relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $save) | Out-Null
    Copy-Item -LiteralPath $target -Destination $save
    if($entry.OriginalHash) {
        Copy-Item -LiteralPath (Join-Path $sourceBackup $entry.Relative) -Destination $target -Force
        if((Get-FileHash -LiteralPath $target).Hash -ne $entry.OriginalHash) { throw 'Restored source mismatch.' }
    } else {
        # Only the two exact, hash-verified files added by this failed change.
        if($entry.Relative -notin @('src/PlantedSwivel.h','tests/recorded_hands_out_regression.inl')) { throw 'Unexpected new file.' }
        Remove-Item -LiteralPath $target
    }
}
$result=foreach($r in $releases) {
    $installed=Join-Path $r.Directory 'TombRaiderVR.dll'
    $ini=Join-Path $r.Directory 'TombRaiderVR.ini'
    $iniHash=(Get-FileHash -LiteralPath $ini).Hash
    $save=Join-Path $backup $r.Id
    New-Item -ItemType Directory -Force -Path $save | Out-Null
    Copy-Item -LiteralPath $installed -Destination $save
    Copy-Item -LiteralPath $ini -Destination $save
    $saved=Join-Path ($deployment | Where-Object Collection -eq $r.Id).Backup 'TombRaiderVR.dll'
    Copy-Item -LiteralPath $saved -Destination $installed -Force
    if((Get-FileHash -LiteralPath $installed).Hash -ne $r.Previous) { throw 'Restored DLL mismatch.' }
    if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) { throw 'INI changed.' }
    [pscustomobject]@{Collection=$r.Id;SHA256=$r.Previous;INIUnchanged=$true;FailedVersionBackup=$save}
}
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'restored.json') -Encoding UTF8
Write-Output 'Restored both previous DLLs and matching TR1-3 source. Hashes verified; INIs unchanged.'
