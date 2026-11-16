@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\turn-reacquire\diagnose.cpp /Febuild\turn-reacquire\diagnose.exe /Fobuild\turn-reacquire\
if errorlevel 1 exit /b 1
