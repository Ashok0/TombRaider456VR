@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\menu-r3-ammo\456\input-tests mkdir build\menu-r3-ammo\456\input-tests
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers build\menu-r3-ammo\input_test.cpp tools\dynamic_bones_regression.cpp /Febuild\menu-r3-ammo\456\input-tests\input_test.exe /Fobuild\menu-r3-ammo\456\input-tests\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\menu-r3-ammo\456\input-tests\input_test.exe




