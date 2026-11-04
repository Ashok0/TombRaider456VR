@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "C:\dev\TombRaider456VR\build\body-position\456\menu-tests"
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I"C:\dev\TombRaider456VR/src" /I"C:\dev\TombRaider456VR/third_party/openvr/headers" "C:\dev\TombRaider456VR/tools/vr_menu_tests.cpp" "C:\dev\TombRaider456VR/src/Log.cpp" /Femenu.exe /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
menu.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I"C:\dev\TombRaider456VR/src" /I"C:\dev\TombRaider456VR/third_party/openvr/headers" "C:\dev\TombRaider456VR/tools/vr_native_menu_tests.cpp" "C:\dev\TombRaider456VR/src/Log.cpp" "C:\dev\TombRaider456VR/src/InlineHook.cpp" /Fenative.exe /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
native.exe
