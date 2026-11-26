@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /Isrc tools/turn_pivot_tests.cpp /Febuild/turn-pivot-response/pivot.exe /Fobuild/turn-pivot-response/pivot.obj
if errorlevel 1 exit /b 1
build\turn-pivot-response\pivot.exe
