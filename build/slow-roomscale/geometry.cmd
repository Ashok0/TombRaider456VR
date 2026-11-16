@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Isrc tools/connected_arm_tests.cpp /Febuild/slow-roomscale/geometry.exe /Fobuild/slow-roomscale/geometry.obj
if errorlevel 1 exit /b 1
build\slow-roomscale\geometry.exe
