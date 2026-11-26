@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /Ibuild/turn-pivot-response/123-work/src tools/turn_pivot_tests.cpp /Febuild/turn-pivot-response/pivot-123.exe /Fobuild/turn-pivot-response/pivot-123.obj
if errorlevel 1 exit /b 1
build\turn-pivot-response\pivot-123.exe
