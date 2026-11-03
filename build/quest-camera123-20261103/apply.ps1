$ErrorActionPreference='Stop'
$targetRoot='C:\dev\TombRaider123VR'
$stageRoot=Join-Path $PSScriptRoot 'stage'
$records=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'source_hashes.json') -Raw | ConvertFrom-Json
foreach($record in $records) {
    $target=Join-Path $targetRoot $record.Relative
    if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $record.Before) { throw "Source changed since staging: $target" }
}
foreach($relative in @('src\FirstPerson.cpp','src\FirstPersonVisibility.h','README.md','tests\quest_camera_test.cpp','tests\build_quest_camera_tests.cmd')) {
    $target=Join-Path $targetRoot $relative
    if($relative -like 'tests\*' -and (Test-Path -LiteralPath $target)) { throw "New fixture already exists: $target" }
    Copy-Item -LiteralPath (Join-Path $stageRoot $relative) -Destination $target -Force
}
Write-Output 'Applied the TR1-3 quest-camera port, focused regression fixture and README update.'
