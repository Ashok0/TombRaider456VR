@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\ladder-exit-facing\motion-audit.cpp /Febuild\ladder-exit-facing\motion-audit.exe /Fobuild\ladder-exit-facing\
if errorlevel 1 exit /b 1
build\ladder-exit-facing\motion-audit.exe
