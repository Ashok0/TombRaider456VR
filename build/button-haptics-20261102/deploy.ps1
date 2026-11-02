$ErrorActionPreference='Stop'
$buildRoot='C:\dev\TombRaider456VR\build\button-haptics-20261102'
$manifest=Get-Content -LiteralPath (Join-Path $buildRoot 'deployment-manifest.json') -Raw | ConvertFrom-Json
if(Get-Process tomb123,tomb456 -ErrorAction SilentlyContinue) { throw 'A game is running; close it before replacing the DLL.' }
foreach($entry in $manifest) {
    if((Get-FileHash -LiteralPath $entry.source).Hash -ne $entry.newHash) { throw 'Built DLL changed after preparation.' }
    if((Get-FileHash -LiteralPath $entry.destination).Hash -ne $entry.oldHash) { throw 'Installed DLL changed after preparation.' }
    if((Get-FileHash -LiteralPath $entry.ini).Hash -ne $entry.iniHash) { throw 'Installed INI changed after preparation.' }
}
$backupRoot=Join-Path $buildRoot ('before-deploy-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
foreach($entry in $manifest) {
    $backup=Join-Path $backupRoot $entry.collection
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    Copy-Item -LiteralPath $entry.destination -Destination $backup
    Copy-Item -LiteralPath $entry.ini -Destination $backup
}
foreach($entry in $manifest) {
    if(Get-Process tomb123,tomb456 -ErrorAction SilentlyContinue) { throw 'A game restarted during deployment.' }
    Copy-Item -LiteralPath $entry.source -Destination $entry.destination -Force
    if((Get-FileHash -LiteralPath $entry.destination).Hash -ne $entry.newHash) { throw 'Deployed DLL hash mismatch.' }
    if((Get-FileHash -LiteralPath $entry.ini).Hash -ne $entry.iniHash) { throw 'INI changed unexpectedly.' }
    Write-Output ('TR'+$entry.collection+': installed and SHA256 verified; INI unchanged.')
}
@{deployed=(Get-Date -Format o);backup=$backupRoot;files=@($manifest);releaseBuildsPassed=$true;fullSelfTestRun=$false;focusedButtonChecks456=29251;focusedKeypadChecks456=16771;focusedButtonChecks123=16976} |
    ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $buildRoot 'deployment.json') -Encoding UTF8
