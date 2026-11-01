@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\flare-test mkdir build\flare-test
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\flare_input_test.cpp tools\dynamic_bones_regression.cpp /Febuild\flare-test\flare_input.exe /Fobuild\flare-test\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\flare-test\flare_input.exe
exit /b %errorlevel%
