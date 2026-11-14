from pathlib import Path
import shutil
root=Path('C:/dev/TombRaider456VR');work=root/'build/landing-camera-height';repo=Path('C:/dev/TombRaider123VR');stage=work/'123-work'
for name in ['src','tests','tools','third_party','TombRaiderVRBindings']:
 if (repo/name).exists():shutil.copytree(repo/name,stage/name,dirs_exist_ok=True)
for name in ['TombRaiderVR.vcxproj','TombRaiderVR.sln','README.md']:shutil.copy2(repo/name,stage/name)
p=stage/'src/FirstPerson.cpp';s=p.read_text();old='if (Cfg().firstPersonMovementStabilization && (state==3 || state==9 || (state>=25 && state<=28)) &&'
assert s.count(old)==1
s=s.replace(old,'if (Cfg().firstPersonMovementStabilization && (state==3 || state==9 || (state>=25 && state<=28) || UseLandingCamera(item)) &&')
s=s.replace('// Keep the calibrated eye through takeoff and the whole ordinary jump,','// Keep the calibrated eye through takeoff, flight and the impact kneel.\n        // The landing neck must not pull the headset down into the folded legs.\n        // Keep the calibrated eye through the whole ordinary jump,')
p.write_text(s)
p=stage/'tests/remaining_parity_selftest.h';s=p.read_text();s=s.replace('Check(eye.y_pos==landingY-(ordinary?700:440),"ordinary landing camera removes native neck dip without changing hard-fall camera");','Check(eye.y_pos==landingY-700,"calibrated landing camera rejects ordinary and hard-fall neck dip");');p.write_text(s)
for name in ['test-arms-456.cmd','build-456.cmd','build-123.cmd']:
 (work/name).write_text((root/'build/landing-floor-fit'/name).read_text().replace('landing-floor-fit','landing-camera-height'))
(work/'camera-stubs.cpp').write_text('namespace tr { void VRMenuShutdown() {} }\n')
(work/'test-camera.cmd').write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\\openvr\\headers tools\\first_person_tests.cpp tools\\dynamic_bones_regression.cpp build\\landing-camera-height\\camera-stubs.cpp /Febuild\\landing-camera-height\\camera-tests.exe /Fobuild\\landing-camera-height\\ /link /OPT:REF user32.lib\nif errorlevel 1 exit /b 1\nbuild\\landing-camera-height\\camera-tests.exe\n')
