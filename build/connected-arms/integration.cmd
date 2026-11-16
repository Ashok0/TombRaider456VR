@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\connected-arms\456\arm-tests mkdir build\connected-arms\456\arm-tests
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Itools /Ithird_party\openvr\headers build\connected-arms\integration.cpp tools\dynamic_bones_regression.cpp /Febuild\connected-arms\456\arm-tests\integration.exe /Fobuild\connected-arms\456\arm-tests\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\connected-arms\456\arm-tests\integration.exe




