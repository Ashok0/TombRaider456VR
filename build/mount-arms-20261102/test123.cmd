@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /IC:\dev\TombRaider123VR\src /IC:\dev\TombRaider123VR\third_party\openvr\headers C:\dev\TombRaider123VR\tests\transition_hands_selftest.cpp C:\dev\TombRaider123VR\src\InlineHook.cpp C:\dev\TombRaider123VR\src\Log.cpp /Fe"%~dp0123\test\transition_hands_test.exe" /Fo"%~dp0123\test\\" /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
"%~dp0123\test\transition_hands_test.exe"
