$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider123VR'
$stage='C:\dev\TombRaider456VR\build\crouch123-port'
$source=Join-Path $root 'src\FirstPerson.cpp'
if((Get-FileHash -LiteralPath $source).Hash -ne '0C73D71071A361FEEBCD5816E102520A8BADC87058876E2B0628A20A8BA6301C') { throw 'TR1-3 source changed; review before replacing.' }
foreach($file in @('tests\crouch_transition_test.cpp','tests\build_crouch_transition_tests.cmd')) {
    if(Test-Path -LiteralPath (Join-Path $root $file)) { throw "New test destination already exists: $file" }
}
$backup=Join-Path $root ('build\crouch-transition\before-source-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $source -Destination $backup
foreach($file in @('src\FirstPerson.cpp','tests\crouch_transition_test.cpp','tests\build_crouch_transition_tests.cmd')) {
    Copy-Item -LiteralPath (Join-Path $stage $file) -Destination (Join-Path $root $file) -Force
}
Write-Output "Applied crouch-transition fix; source backup: $backup"
