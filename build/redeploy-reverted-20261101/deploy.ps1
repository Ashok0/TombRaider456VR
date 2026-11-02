$ErrorActionPreference='Stop'
$buildRoot='C:\dev\TombRaider456VR\build\redeploy-reverted-20261101'
$manifest=Get-Content -LiteralPath (Join-Path $buildRoot 'manifest.json') -Raw | ConvertFrom-Json
if(Get-Process tomb123,tomb456 -ErrorAction SilentlyContinue) { throw 'Close the games before deployment.' }
foreach($entry in $manifest) {
    if((Get-FileHash -LiteralPath $entry.ini).Hash -ne $entry.iniHash) { throw 'INI changed during preparation.' }
    foreach($file in $entry.files) {
        if((Get-FileHash -LiteralPath $file.source).Hash -ne $file.expectedNew) { throw 'Built artifact changed.' }
        if($null -eq $file.expectedOld) {
            if(Test-Path -LiteralPath $file.destination) { throw 'A new destination appeared during preparation.' }
        } elseif(!(Test-Path -LiteralPath $file.destination) -or (Get-FileHash -LiteralPath $file.destination).Hash -ne $file.expectedOld) {
            throw 'Installed file changed during preparation.'
        }
    }
}
$backupRoot=Join-Path $buildRoot ('before-redeploy-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
foreach($entry in $manifest) {
    $backup=Join-Path $backupRoot $entry.collection
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    Copy-Item -LiteralPath $entry.ini -Destination $backup
    foreach($file in $entry.files) {
        if($null -ne $file.expectedOld -and $file.expectedOld -ne $file.expectedNew) {
            $saved=Join-Path $backup $file.relative
            New-Item -ItemType Directory -Path (Split-Path -Parent $saved) -Force | Out-Null
            Copy-Item -LiteralPath $file.destination -Destination $saved
        }
    }
}
foreach($entry in $manifest) {
    foreach($file in $entry.files) {
        if($file.expectedNew -ne $file.expectedOld) {
            if(Get-Process tomb123,tomb456 -ErrorAction SilentlyContinue) { throw 'A game restarted during deployment.' }
            Copy-Item -LiteralPath $file.source -Destination $file.destination -Force
        }
        if((Get-FileHash -LiteralPath $file.destination).Hash -ne $file.expectedNew) { throw 'Deployed file verification failed.' }
    }
    if((Get-FileHash -LiteralPath $entry.ini).Hash -ne $entry.iniHash) { throw 'INI changed unexpectedly.' }
    Write-Output ('TR'+$entry.collection+': deployed and SHA256 verified; INI unchanged.')
}
@{deployed=(Get-Date -Format o);backup=$backupRoot;manifest=$manifest;fullSelfTestRun=$false;bothReleaseBuildsPassed=$true;ledgeShortcutAbsent=$true;keypadLightingRetained=$true} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $buildRoot 'deployment.json') -Encoding UTF8
Write-Output ('Backups: '+$backupRoot)
