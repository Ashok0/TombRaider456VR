$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider123VR'
$stage='C:\dev\TombRaider456VR\build\flare123-port'
$hashes=Get-Content -LiteralPath (Join-Path $stage 'original-hashes.json') -Raw | ConvertFrom-Json
foreach($property in $hashes.PSObject.Properties) {
    if((Get-FileHash -LiteralPath (Join-Path $root $property.Name)).Hash -ne $property.Value) {throw "Source changed: $($property.Name)"}
}
$added=@('src\FlareThrowMotion.h','src\FlareThrowAddresses.inl','src\PhysicalFlareThrow.inl','tests\flare_throw_test.cpp','tests\build_flare_tests.cmd')
foreach($file in $added) {if(Test-Path -LiteralPath (Join-Path $root $file)) {throw "New destination exists: $file"}}
$backup=Join-Path $root ('build\physical-flare\before-source-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
foreach($property in $hashes.PSObject.Properties) {
    $save=Join-Path $backup $property.Name
    New-Item -ItemType Directory -Path (Split-Path -Parent $save) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $property.Name) -Destination $save
}
$files=@($hashes.PSObject.Properties.Name)+$added
foreach($file in $files) {Copy-Item -LiteralPath (Join-Path $stage $file) -Destination (Join-Path $root $file) -Force}
Write-Output "Applied physical flare port; original source saved in $backup"
