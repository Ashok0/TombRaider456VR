@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "C:\dev\TombRaider123VR"
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party/openvr/headers tests/button_touch_selftest.cpp src/InlineHook.cpp src/Log.cpp /Fe"C:\dev\TombRaider456VR\build\button-haptics-20261102\buttons123/test.exe" /Fo"C:\dev\TombRaider456VR\build\button-haptics-20261102\buttons123/" /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
"C:\dev\TombRaider456VR\build\button-haptics-20261102\buttons123/test.exe"
