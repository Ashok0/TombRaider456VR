$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider123VR'
$stage='C:\dev\TombRaider456VR\build\ground-queue123-port'
$source=Join-Path $root 'src\FirstPerson.cpp'
$readme=Join-Path $root 'README.md'
if((Get-FileHash -LiteralPath $source).Hash -ne 'C392BDB467C8FA60A4587357CCECAADAD4C641066E358C74E47549EE46A199F7') { throw 'TR1-3 source changed; review before replacing.' }
if((Get-FileHash -LiteralPath $readme).Hash -ne 'BEF039922C996686BD2EE9FBF0DC5B99FFB1379CA2AA1C120F527C27C7F18A4E') { throw 'TR1-3 README changed; review before replacing.' }
foreach($file in @('tests\ground_queue_test.cpp','tests\build_ground_queue_tests.cmd')) {
    if(Test-Path -LiteralPath (Join-Path $root $file)) { throw "Test destination already exists: $file" }
}
$backup=Join-Path $root ('build\ground-queue\before-source-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $source -Destination $backup
Copy-Item -LiteralPath $readme -Destination $backup
foreach($file in @('src\FirstPerson.cpp','README.md','tests\ground_queue_test.cpp','tests\build_ground_queue_tests.cmd')) {
    Copy-Item -LiteralPath (Join-Path $stage $file) -Destination (Join-Path $root $file) -Force
}
Write-Output "Applied TR3 ground-queue correction; source backup: $backup"
