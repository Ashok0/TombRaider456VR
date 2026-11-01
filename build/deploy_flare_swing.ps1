$ErrorActionPreference='Stop'
$root='C:\dev\TombRaider456VR'
$steam='C:\Program Files (x86)\Steam\steamapps\common'
$builds=@(
    @{id='123';process='tomb123';folder='Tomb Raider I-III Remastered';source='C:\dev\TombRaider123VR\build\physical-flare\bin\TombRaiderVR.dll';old='028C0F8D96C52057B9C0E5A7D9D252B17B3B7734B0D6AF37BDF4F4942D437552';new='DFBF37894A35446B44251DA16A7B0C6802DEF48D0289996B21F43248758FC4DC'},
    @{id='456';process='tomb456';folder='Tomb Raider IV-VI Remastered';source='C:\dev\TombRaider456VR\build\vr-options-release\TombRaiderVR.dll';old='346A7E034A11EC8668DAA07E1C6AB253749253D11E2BA2E909EBC3608C4CA55B';new='71A3352488B819ED3D74A057F6ED4971E414EB093AC55121D00256C9AD097153'}
)
foreach($b in $builds) {
    if(Get-Process $b.process -ErrorAction SilentlyContinue) {throw "Close $($b.process) before installation."}
    $b.destination=Join-Path (Join-Path $steam $b.folder) 'TombRaiderVR.dll'
    $b.ini=Join-Path (Join-Path $steam $b.folder) 'TombRaiderVR.ini'
    if((Get-FileHash -LiteralPath $b.source).Hash -ne $b.new) {throw 'Tested build changed.'}
    if((Get-FileHash -LiteralPath $b.destination).Hash -ne $b.old) {throw 'Installed DLL changed.'}
    $b.iniHash=(Get-FileHash -LiteralPath $b.ini).Hash
}
$backup=Join-Path $root ('build\physical-flare\before-swing-install-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
foreach($b in $builds) {
    $save=Join-Path $backup $b.id
    New-Item -ItemType Directory -Path $save -Force | Out-Null
    Copy-Item -LiteralPath $b.destination -Destination $save
    Copy-Item -LiteralPath $b.ini -Destination $save
}
foreach($b in $builds) {
    if(Get-Process $b.process -ErrorAction SilentlyContinue) {throw 'Game started during preparation.'}
    Copy-Item -LiteralPath $b.source -Destination $b.destination -Force
    if((Get-FileHash -LiteralPath $b.destination).Hash -ne $b.new) {throw 'Installed DLL verification failed.'}
    if((Get-FileHash -LiteralPath $b.ini).Hash -ne $b.iniHash) {throw 'INI changed unexpectedly.'}
    Write-Output "Installed TR$($b.id) physical flare build; INI unchanged: $($b.new)"
}
$record=@{deployed=(Get-Date -Format o);backup=$backup;builds=$builds;
    release_builds_passed=$true;flare_checks=11456;controller_integration_checks=73;
    native_release_unchanged=$true;offline_verified_dlls=10;
    full_self_test_run=$false;headset_tested=$false;
    behavior='TR2-5 first-person Remastered tracked flare: automatic deliberate left-controller swing; left grip remains crouch; head-relative intent rejects physical crouching and roomscale translation; native release/cleanup unchanged'
}
$record | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $root 'build\physical-flare\swing-deployment.json') -Encoding UTF8
Write-Output "Previous DLLs and INIs backed up in $backup"
