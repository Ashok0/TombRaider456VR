$ErrorActionPreference='Stop'
$old='C:\dev\TombRaider456VR\build\body-neck-seal'
$manifest=Get-Content -LiteralPath (Join-Path $old 'source123-manifest.json') -Raw | ConvertFrom-Json
$releases=Get-Content -LiteralPath (Join-Path $old 'releases.json') -Raw | ConvertFrom-Json
$installed=Get-Content -LiteralPath (Join-Path $old 'deployment.json') -Raw | ConvertFrom-Json
foreach($release in $releases) {
 if(Get-Process $release.Process -ErrorAction SilentlyContinue) {throw 'Game is running; close before rollback.'}
 $dll=Join-Path $release.Directory 'TombRaiderVR.dll'
 if((Get-FileHash -LiteralPath $dll).Hash -ne $release.Expected) {throw 'Installed DLL changed since tested deployment.'}
}
foreach($entry in $manifest) {
 $target=Join-Path 'C:\dev\TombRaider123VR' $entry.Relative
 if((Get-FileHash -LiteralPath $target).Hash -ne $entry.ModifiedHash) {throw 'TR123 source changed since deployment.'}
}
$before=Split-Path -Parent $installed[0].Backup
foreach($entry in $manifest) {
 $target=Join-Path 'C:\dev\TombRaider123VR' $entry.Relative
 if($entry.OriginalHash) {
  $backup=Join-Path (Join-Path $before '123-source') $entry.Relative
  if((Get-FileHash -LiteralPath $backup).Hash -ne $entry.OriginalHash) {throw 'Source backup mismatch.'}
  Copy-Item -LiteralPath $backup -Destination $target -Force
 } else { Remove-Item -LiteralPath $target }
}
$results=foreach($release in $releases) {
 $backup=Join-Path (Join-Path $before $release.Id) 'TombRaiderVR.dll'
 if((Get-FileHash -LiteralPath $backup).Hash -ne $release.Previous) {throw 'Previous DLL backup mismatch.'}
 $ini=Join-Path $release.Directory 'TombRaiderVR.ini';$iniHash=(Get-FileHash -LiteralPath $ini).Hash
 $dll=Join-Path $release.Directory 'TombRaiderVR.dll'
 Copy-Item -LiteralPath $backup -Destination $dll -Force
 if((Get-FileHash -LiteralPath $dll).Hash -ne $release.Previous) {throw 'Rollback DLL mismatch.'}
 if((Get-FileHash -LiteralPath $ini).Hash -ne $iniHash) {throw 'INI changed.'}
 [pscustomobject]@{Collection=$release.Id;Installed=$dll;SHA256=$release.Previous;INIUnchanged=$true}
}
$results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'rollback.json') -Encoding UTF8
Write-Output 'Restored both pre-cap builds and TR1-3 sources. Height fix and current INI settings preserved.'
