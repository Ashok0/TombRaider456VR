$ErrorActionPreference = 'Stop'
$workspace = 'C:\dev\TombRaider456VR'
$stage = Join-Path $workspace 'build\physical-crouch123'
$destination = 'C:\dev\TombRaider123VR'
$release = Join-Path $workspace 'build\physical-crouch123-release'
$game = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered'
if (Get-Process | Where-Object { $_.ProcessName -eq 'tomb123' -or ($_.Path -and $_.Path.StartsWith($game + '\', [StringComparison]::OrdinalIgnoreCase)) }) {
    throw 'Close the game before deployment.'
}
function Hash([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function SafePath([string]$root,[string]$relative) {
    $path = [IO.Path]::GetFullPath((Join-Path $root $relative))
    if (!$path.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Path escaped its root.' }
    return $path
}
$rows = Get-Content -LiteralPath (Join-Path $stage 'port-files.json') -Raw | ConvertFrom-Json
$original = Get-Content -LiteralPath (Join-Path $stage 'original-hashes.json') -Raw | ConvertFrom-Json
foreach ($entry in $original.PSObject.Properties) {
    $path = SafePath $destination $entry.Name
    if (!(Test-Path -LiteralPath $path) -or (Hash $path) -ne $entry.Value) { throw "Source changed since staging: $($entry.Name)" }
}
foreach ($row in $rows) {
    $source = SafePath $stage $row.path
    $target = SafePath $destination $row.path
    if ((Hash $source) -ne $row.new_sha256) { throw "Staged file changed: $($row.path)" }
    if (!$row.old_sha256 -and (Test-Path -LiteralPath $target)) { throw "New file already exists: $($row.path)" }
}
$builtDll = Join-Path $release 'TombRaiderVR.dll'
$installedDll = Join-Path $game 'TombRaiderVR.dll'
$installedIni = Join-Path $game 'TombRaiderVR.ini'
$builtHash = Hash $builtDll
if ($builtHash -ne 'FCD859569F4F9201E149BF10E5E171194E462EC031CBFF8E758B7B916A3D8B20') { throw 'Unexpected built DLL.' }
if ((Hash $installedDll) -ne '97948BB62F83F17B340A4130CB5FECF61F4B50D67F2F63B8FD68719822F9E1F1') { throw 'Installed DLL changed since review.' }
$oldIniHash = Hash $installedIni
if ($oldIniHash -ne 'D7AEEFA0016D3ABF52196987E7EEAA9A5B6DEA7ED3332F2D5E84368DC6036E96') { throw 'Installed INI changed since review.' }
$encoding = New-Object Text.UTF8Encoding($false)
$iniText = $encoding.GetString([IO.File]::ReadAllBytes($installedIni))
if ($iniText -match '(?im)^FirstPersonPhysicalCrouch\s*=') { throw 'Physical crouch key unexpectedly exists.' }
if ($iniText -notmatch '(?im)^FirstPersonFullBodyIK=1\r*$') { throw 'Full-body IK prerequisite is not enabled.' }
$sections = [regex]::Matches($iniText, '(?m)^\[VR\]\r*\n')
if ($sections.Count -ne 1) { throw 'Expected one VR section.' }
$newline = if ($iniText.Contains("`r`n")) { "`r`n" } else { "`n" }
$nextIni = $iniText.Insert($sections[0].Index + $sections[0].Length, 'FirstPersonPhysicalCrouch=1' + $newline)
$backup = Join-Path $workspace ('build\before-physical-crouch123-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
foreach ($row in $rows) {
    if ($row.old_sha256) {
        $saved = SafePath $backup ('source\' + $row.path)
        New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($saved)) -Force | Out-Null
        Copy-Item -LiteralPath (SafePath $destination $row.path) -Destination $saved
    }
}
foreach ($name in @('TombRaiderVR.dll','TombRaiderVR.ini','TombRaiderVR.log')) {
    $path = Join-Path $game $name
    if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination $backup }
}
foreach ($row in $rows) {
    Copy-Item -LiteralPath (SafePath $stage $row.path) -Destination (SafePath $destination $row.path) -Force
}
Copy-Item -LiteralPath $builtDll -Destination $installedDll -Force
[IO.File]::WriteAllBytes($installedIni,$encoding.GetBytes($nextIni))
foreach ($row in $rows) {
    if ((Hash (SafePath $destination $row.path)) -ne $row.new_sha256) { throw "Applied source mismatch: $($row.path)" }
}
if ((Hash $installedDll) -ne $builtHash) { throw 'Installed DLL mismatch.' }
if ($encoding.GetString([IO.File]::ReadAllBytes($installedIni)) -ne $nextIni) { throw 'Installed INI mismatch.' }
$manifest = [ordered]@{
    status = 'applied-deployed-and-hash-verified'
    deployed_at = (Get-Date).ToString('o')
    source_repository = $destination
    source_files = @($rows.path)
    dll_sha256 = $builtHash
    old_ini_sha256 = $oldIniHash
    ini_sha256 = Hash $installedIni
    backup = $backup
    setting_change = 'FirstPersonPhysicalCrouch=1 added; other installed INI content preserved'
    validation = 'Release build and 160 standalone posture/input checks passed; 15 read-only native comparisons passed; full self-test not run; headset unverified'
}
$json = $manifest | ConvertTo-Json -Depth 4
[IO.File]::WriteAllText((Join-Path $release 'manifest.json'),$json,$encoding)
$evidence = Join-Path $destination 'build\physical-crouch-port'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $evidence 'manifest.json'),$json,$encoding)
Write-Output $json
