@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /I"C:\dev\TombRaider123VR/src" /I"C:\dev\TombRaider123VR/third_party/openvr/headers" "C:\dev\TombRaider123VR/tests/back_hop_test.cpp" "C:\dev\TombRaider123VR/src/InlineHook.cpp" "C:\dev\TombRaider123VR/src/Log.cpp" /Fe"C:\dev\TombRaider456VR\build\hybrid-controls-20261102\123/tests/test.exe" /Fo"C:\dev\TombRaider456VR\build\hybrid-controls-20261102\123/tests/" /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
"C:\dev\TombRaider456VR\build\hybrid-controls-20261102\123/tests/test.exe"
