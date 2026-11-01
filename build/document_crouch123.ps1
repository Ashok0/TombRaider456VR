$ErrorActionPreference='Stop'
$file='C:\dev\TombRaider123VR\README.md'
if((Get-FileHash -LiteralPath $file).Hash -ne 'B80B274E85C29FA799A22C286004EDAAB238315A6A8997A87D7CC3A87B0C6AF0') { throw 'README changed; review before editing.' }
$text=[IO.File]::ReadAllText($file)
$needle='No game DLL code was executed by that check. Headset behavior remains unverified.'
if($text.IndexOf($needle) -lt 0 -or $text.IndexOf($needle) -ne $text.LastIndexOf($needle)) { throw 'Expected documentation anchor not unique.' }
$addition=@'


2026-11-01 TR3 crouch-transition correction: ordinary queued crouch/crawl
postures keep headset-relative steering. Native all-fours movement marks Lara's
hands busy; those normal crouch states now retain tracked hands and body IK.
The fix preserves the queued posture, native ceiling/wall checks and guards for
interactions, hangs, rolls and vehicles. TR1/2 have no native crawl states;
their existing physical-squat body hiding is unchanged.

The Release build and 5,490 focused checks in `tests/crouch_transition_test.cpp`
passed, including production movement hooks with synthetic native callbacks,
both controller targets, standing release and TR1/2 exclusions. The test
reproduced the queued-posture failure before the fix. No game DLL code or full
self-test was run. Headset confirmation is still needed.
'@
$addition=$addition.Replace("`r`n","`n").Replace("`n","`r`n")
[IO.File]::WriteAllText($file,$text.Replace($needle,$needle+$addition),[Text.UTF8Encoding]::new($false))
Write-Output 'Documented TR3 crouch-transition port and focused validation.'
