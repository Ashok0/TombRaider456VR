@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\turn-pivot-response\456\arm-tests mkdir build\turn-pivot-response\456\arm-tests
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Itools /Ithird_party\openvr\headers build\turn-pivot-response\integration.cpp tools\dynamic_bones_regression.cpp /Febuild\turn-pivot-response\456\arm-tests\integration.exe /Fobuild\turn-pivot-response\456\arm-tests\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\turn-pivot-response\456\arm-tests\integration.exe




