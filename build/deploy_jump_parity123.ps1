$ErrorActionPreference = 'Stop'
$workspace = 'C:\dev\TombRaider456VR'
$stage = Join-Path $workspace 'build\jump-parity123'
$repository = 'C:\dev\TombRaider123VR'
$gameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider I-III Remastered'
$builtDll = Join-Path $workspace 'build\jump-parity123-release\TombRaiderVR.dll'
$installedDll = Join-Path $gameDirectory 'TombRaiderVR.dll'
$installedIni = Join-Path $gameDirectory 'TombRaiderVR.ini'
$files = Get-Content -LiteralPath (Join-Path $stage 'apply-manifest.json') -Raw | ConvertFrom-Json
if (Get-Process | Where-Object { $_.ProcessName -eq 'tomb123' -or ($_.Path -and $_.Path.StartsWith($gameDirectory + '\', [StringComparison]::OrdinalIgnoreCase)) }) {
    throw 'Close TR1-3 before installing the port.'
}
$dllHash = (Get-FileHash -LiteralPath $builtDll -Algorithm SHA256).Hash
if ($dllHash -ne 'DF292BC8B5B80FB6671F3901C8AF6F5F38FDCE482539EDE17E01918645F1D357') {
    throw 'Built DLL changed since validation.'
}
foreach ($entry in $files) {
    $source = [IO.Path]::GetFullPath((Join-Path $stage $entry.path))
    $target = [IO.Path]::GetFullPath((Join-Path $repository $entry.path))
    if (!$source.StartsWith($stage + '\', [StringComparison]::OrdinalIgnoreCase) -or !$target.StartsWith($repository + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Manifest path escapes the intended source directory.'
    }
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $entry.after) { throw "Staged source changed: $($entry.path)" }
    if ($entry.before) {
        if (!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $entry.before) {
            throw "Existing user source changed during the port: $($entry.path)"
        }
    } elseif (Test-Path -LiteralPath $target) { throw "New file would overwrite existing work: $($entry.path)" }
}
$iniHash = (Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash
$backup = Join-Path $workspace ('build\before-jump-parity123-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
foreach ($entry in $files) {
    if ($entry.before) {
        $destination = Join-Path (Join-Path $backup 'source') $entry.path
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $repository $entry.path) -Destination $destination
    }
}
foreach ($name in @('TombRaiderVR.dll','TombRaiderVR.ini','TombRaiderVR.log')) {
    $path = Join-Path $gameDirectory $name
    if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination $backup }
}
foreach ($entry in $files) {
    Copy-Item -LiteralPath (Join-Path $stage $entry.path) -Destination (Join-Path $repository $entry.path) -Force
    if ((Get-FileHash -LiteralPath (Join-Path $repository $entry.path) -Algorithm SHA256).Hash -ne $entry.after) {
        throw "Applied source hash mismatch: $($entry.path)"
    }
}
$evidence = Join-Path $repository 'build\jump-parity-port'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
foreach ($name in @('apply-manifest.json','asset-verification.log','binary-verification.log','release-build.log','initial-tests.log','jump-port-tests.log')) {
    Copy-Item -LiteralPath (Join-Path $stage $name) -Destination $evidence -Force
}
Copy-Item -LiteralPath $builtDll -Destination $evidence -Force
Copy-Item -LiteralPath $builtDll -Destination $installedDll -Force
if ((Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash -ne $dllHash) { throw 'Installed DLL hash mismatch.' }
if ((Get-FileHash -LiteralPath $installedIni -Algorithm SHA256).Hash -ne $iniHash) { throw 'Installed settings changed.' }
$manifest = [ordered]@{
    status = 'source-applied-and-game-DLL-installed'
    deployed_at = (Get-Date).ToString('o')
    repository = $repository
    destination = $gameDirectory
    dll_sha256 = $dllHash
    ini_sha256 = $iniHash
    backup = $backup
    source_files = @($files.path)
    release_build = 'passed'
    asset_checks = '3920 assertions across all 140 gameplay animation banks passed'
    binary_checks = '15 fixtures / nine unique DLL builds passed'
    native_direction_probe = '360 cases passed; armed backward-to-side error reproduced in all three games'
    existing_suite = '1152967 checks passed before adding the expanded jump suite'
    expanded_selftest = 'incomplete: synthetic camera fixture crashed; further runs stopped at user request; fixture pointer corrected without rerunning'
    headset_verified = $false
    changes = 'Gaps 1-9: preparation posture, immediate takeoff, matching ledge timing, soft landing recovery, playable camera overrides, held-jump persistence, retained Modern Controls, side-jump camera stabilization, armed directional takeoff guard.'
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $evidence 'deployment.json') -Encoding UTF8
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'deployment.json') -Encoding UTF8
$manifest | ConvertTo-Json -Depth 5
