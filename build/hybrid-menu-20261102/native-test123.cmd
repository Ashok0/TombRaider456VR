@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "C:\dev\TombRaider456VR\build\hybrid-menu-20261102\123\native-test"
cl /nologo /std:c++17 /O2 /Gy /EHsc /W4 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /I"C:\dev\TombRaider123VR/src" /I"C:\dev\TombRaider123VR/third_party/openvr/headers" "C:\dev\TombRaider123VR/tools/vr_native_menu_tests.cpp" "C:\dev\TombRaider123VR/src/Log.cpp" "C:\dev\TombRaider123VR/src/InlineHook.cpp" /Fe"C:\dev\TombRaider456VR\build\hybrid-menu-20261102\123\native-test/test.exe" /Fo"C:\dev\TombRaider456VR\build\hybrid-menu-20261102\123\native-test/" /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
"C:\dev\TombRaider456VR\build\hybrid-menu-20261102\123\native-test/test.exe"
