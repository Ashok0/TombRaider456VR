@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /Isrc tools\connected_arm_tests.cpp /Febuild\connected-arms\geometry.exe /Fobuild\connected-arms\
if errorlevel 1 exit /b 1
build\connected-arms\geometry.exe
