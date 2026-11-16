@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /Isrc tools\connected_arm_tests.cpp /Febuild\soft-arm-reach\geometry.exe /Fobuild\soft-arm-reach\geometry.obj
if errorlevel 1 exit /b 1
build\soft-arm-reach\geometry.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc tools\vr_menu_tests.cpp src\Log.cpp /Febuild\soft-arm-reach\menu.exe /Fobuild\soft-arm-reach\ /link user32.lib
if errorlevel 1 exit /b 1
pushd build\soft-arm-reach
menu.exe
popd
