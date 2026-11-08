@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\roomscale-intent\probe.cpp /Febuild\roomscale-intent\probe.exe /Fobuild\roomscale-intent\probe.obj
if errorlevel 1 exit /b 1
build\roomscale-intent\probe.exe
