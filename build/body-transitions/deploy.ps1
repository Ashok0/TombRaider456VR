$ErrorActionPreference='Stop'
$repo123='C:\dev\TombRaider123VR'
$stage=Join-Path $PSScriptRoot '123-work'
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'source123-manifest.json') -Raw | ConvertFrom-Json
$releases=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'releases.json') -Raw | ConvertFrom-Json
foreach($entry in $manifest) {
    $target=Join-Path $repo123 $entry.Relative
    if($entry.OriginalHash) {
        if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $entry.OriginalHash) { throw 'TR1-3 source changed since staging.' }
    } elseif(Test-Path -LiteralPath $target) { throw 'New source path is already occupied.' }
    if((Get-FileHash -LiteralPath (Join-Path $stage $entry.Relative) -Algorithm SHA256).Hash -ne $entry.ModifiedHash) { throw 'Tested source changed.' }
}
foreach($release in $releases) {
    if(Get-Process $release.Process -ErrorAction SilentlyContinue) { throw 'Close the game before installation.' }
    $built=Join-Path $PSScriptRoot ($release.Id+'\bin\TombRaiderVR.dll')
    if((Get-FileHash -LiteralPath $built -Algorithm SHA256).Hash -ne $release.Expected) { throw 'Tested DLL changed.' }
    if((Get-FileHash -LiteralPath (Join-Path $release.Directory 'TombRaiderVR.dll') -Algorithm SHA256).Hash -ne $release.Previous) { throw 'Installed DLL changed.' }
}
$backup=Join-Path $PSScriptRoot ('before-install-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
foreach($release in $releases) {
    $directory=Join-Path $backup $release.Id
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    foreach($file in @('TombRaiderVR.dll','TombRaiderVR.ini')) {
        Copy-Item -LiteralPath (Join-Path $release.Directory $file) -Destination $directory
    }
}
foreach($entry in $manifest) {
    if($entry.OriginalHash) {
        $save=Join-Path (Join-Path $backup '123-source') $entry.Relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $save) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $repo123 $entry.Relative) -Destination $save
    }
    Copy-Item -LiteralPath (Join-Path $stage $entry.Relative) -Destination (Join-Path $repo123 $entry.Relative) -Force
    if((Get-FileHash -LiteralPath (Join-Path $repo123 $entry.Relative) -Algorithm SHA256).Hash -ne $entry.ModifiedHash) { throw 'Source copy mismatch.' }
}
$repoBuild=Join-Path $repo123 'build\body-transitions\bin'
New-Item -ItemType Directory -Path $repoBuild -Force | Out-Null
foreach($file in @('TombRaiderVR.dll','TombRaiderVR.pdb')) {
    Copy-Item -LiteralPath (Join-Path (Join-Path $PSScriptRoot '123\bin') $file) -Destination $repoBuild -Force
}
$results=foreach($release in $releases) {
    $installed=Join-Path $release.Directory 'TombRaiderVR.dll'
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot ($release.Id+'\bin\TombRaiderVR.dll')) -Destination $installed -Force
    if((Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash -ne $release.Expected) { throw 'Installed DLL mismatch.' }
    $ini=Join-Path $release.Directory 'TombRaiderVR.ini'
    $savedINI=Join-Path (Join-Path $backup $release.Id) 'TombRaiderVR.ini'
    if((Get-FileHash -LiteralPath $ini).Hash -ne (Get-FileHash -LiteralPath $savedINI).Hash) { throw 'INI changed.' }
    [pscustomobject]@{Collection=$release.Id;Installed=$installed;SHA256=$release.Expected;Backup=(Join-Path $backup $release.Id);INIUnchanged=$true;RenderChecks=$release.RenderChecks;InHeadsetTested=$false}
}
$results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'deployment.json') -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'deployment.json') -Destination (Join-Path $repo123 'build\body-transitions\deployment.json') -Force
Write-Output 'Installed both tested builds. Source and DLL hashes verified; existing INIs preserved; previous DLLs backed up.'
