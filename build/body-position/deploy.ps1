$ErrorActionPreference='Stop'
$repo123='C:\dev\TombRaider123VR'
$repo456='C:\dev\TombRaider456VR'
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'source123-manifest.json') -Raw | ConvertFrom-Json
$releases=@(
    @{Id='123';Process='tomb123';Game='Tomb Raider I-III Remastered';Previous='1DB5EAE13918A906E9557E83011C5AC5ED9212FA5E7FE67290CD0384FAE84F3F';Expected='9D5D12DD7FA141CF687C028158C8EA8FECC857FC3C895590D5779A1644AA0C98'},
    @{Id='456';Process='tomb456';Game='Tomb Raider IV-VI Remastered';Previous='83312220679E66D0C2524BE1023CD92CD24FA4AE5BEB1EFB4E702CB26D8A9EE5';Expected='964EA1F381C84E8F1F4378C96ED3E8FF39EFCCBFAE9C0CDF2810DB7E27B3FCD5'}
)
foreach($entry in $manifest) {
    if((Get-FileHash -LiteralPath (Join-Path $repo123 $entry.Relative) -Algorithm SHA256).Hash -ne $entry.OriginalHash) { throw 'TR1-3 source changed since staging.' }
    foreach($root in @((Join-Path $PSScriptRoot '123-work'))) {
        if((Get-FileHash -LiteralPath (Join-Path $root $entry.Relative) -Algorithm SHA256).Hash -ne $entry.ModifiedHash) { throw 'Tested source changed.' }
    }
}
foreach($release in $releases) {
    if(Get-Process $release.Process -ErrorAction SilentlyContinue) { throw "Close $($release.Game) before installation." }
    $release.Directory=Join-Path 'C:\Program Files (x86)\Steam\steamapps\common' $release.Game
    $release.Built=Join-Path $PSScriptRoot ($release.Id+'\bin\TombRaiderVR.dll')
    $release.Installed=Join-Path $release.Directory 'TombRaiderVR.dll'
    $release.INI=Join-Path $release.Directory 'TombRaiderVR.ini'
    if((Get-FileHash -LiteralPath $release.Built -Algorithm SHA256).Hash -ne $release.Expected) { throw 'Tested DLL changed.' }
    if((Get-FileHash -LiteralPath $release.Installed -Algorithm SHA256).Hash -ne $release.Previous) { throw 'Installed DLL changed since inspection.' }
    $release.INIHash=(Get-FileHash -LiteralPath $release.INI -Algorithm SHA256).Hash
}
$backup=Join-Path $PSScriptRoot ('before-install-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
foreach($release in $releases) {
    $directory=Join-Path $backup $release.Id
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    Copy-Item -LiteralPath $release.Installed -Destination $directory
    Copy-Item -LiteralPath $release.INI -Destination $directory
}
foreach($entry in $manifest) {
    $save=Join-Path (Join-Path $backup '123-source') $entry.Relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $save) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repo123 $entry.Relative) -Destination $save
}
foreach($entry in $manifest) {
    Copy-Item -LiteralPath (Join-Path (Join-Path $PSScriptRoot '123-work') $entry.Relative) -Destination (Join-Path $repo123 $entry.Relative) -Force
    if((Get-FileHash -LiteralPath (Join-Path $repo123 $entry.Relative) -Algorithm SHA256).Hash -ne $entry.ModifiedHash) { throw 'TR1-3 source copy mismatch.' }
}
$repoBuild=Join-Path $repo123 'build\body-position\bin'
New-Item -ItemType Directory -Path $repoBuild -Force | Out-Null
foreach($file in @('TombRaiderVR.dll','TombRaiderVR.pdb')) {
    Copy-Item -LiteralPath (Join-Path (Join-Path $PSScriptRoot '123\bin') $file) -Destination $repoBuild -Force
}
$results=foreach($release in $releases) {
    Copy-Item -LiteralPath $release.Built -Destination $release.Installed -Force
    if((Get-FileHash -LiteralPath $release.Installed -Algorithm SHA256).Hash -ne $release.Expected) { throw 'Installed DLL hash mismatch.' }
    if((Get-FileHash -LiteralPath $release.INI -Algorithm SHA256).Hash -ne $release.INIHash) { throw 'Game INI changed.' }
    [pscustomobject]@{Collection=$release.Id;Installed=$release.Installed;SHA256=$release.Expected;Backup=(Join-Path $backup $release.Id);INIUnchanged=$true;NativeMenuChecks=298;SettingsChecks=101;RenderChecks=$(if($release.Id -eq '123'){1248242}else{1633293});InHeadsetTested=$false}
}
$results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'deployment.json') -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'deployment.json') -Destination (Join-Path $repo123 'build\body-position\deployment.json') -Force
Write-Output 'Installed live Body Position adjustment in both collections. Previous DLLs backed up, installed hashes verified, both INIs unchanged.'
