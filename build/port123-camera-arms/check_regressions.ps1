$ErrorActionPreference = 'Stop'
$source = 'C:\dev\TombRaider123VR\src\FirstPerson.cpp'
$original = [IO.File]::ReadAllBytes($source)
$text = [Text.Encoding]::UTF8.GetString($original)
$mutations = @(
    @('gait-before.log','g_groundGait.Apply(native,gaitRoot,sideBack,mainBody','g_groundGait.Apply(native,gaitRoot,false,mainBody','side/back production arm palettes reject native torso sway'),
    @('crate-before.log','    ClampHeadToCollision(item, body, renderedHead, collideTracking ? eyeY : double(pose.y_pos),capsuleHeight);','    if(collideTracking) ClampHeadToCollision(item, body, renderedHead, eyeY,capsuleHeight);','base camera still stops at geometry')
)
Push-Location 'C:\dev\TombRaider123VR'
try {
    foreach ($mutation in $mutations) {
        if (-not $text.Contains($mutation[1])) { throw 'Missing regression mutation target' }
        try {
            [IO.File]::WriteAllText($source,$text.Replace($mutation[1],$mutation[2]),[Text.UTF8Encoding]::new($false))
            $log = Join-Path $PSScriptRoot $mutation[0]
            $command = 'tests\build_locomotion_selftest.cmd > "' + $log + '" 2>&1'
            cmd /c $command
            if ($LASTEXITCODE -eq 0 -or -not (Select-String -LiteralPath $log -SimpleMatch $mutation[3])) {
                throw "Expected regression was not detected: $($mutation[0])"
            }
            Get-Content -LiteralPath $log -Tail 1
        } finally { [IO.File]::WriteAllBytes($source,$original) }
    }
    $command = 'tests\build_locomotion_selftest.cmd > "' + (Join-Path $PSScriptRoot 'locomotion.log') + '" 2>&1'
    cmd /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Restored production regression suite failed' }
    Get-Content -LiteralPath (Join-Path $PSScriptRoot 'locomotion.log') -Tail 1
} finally {
    [IO.File]::WriteAllBytes($source,$original)
    Pop-Location
}
