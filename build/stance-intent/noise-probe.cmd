@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\stance-intent\noise-probe.cpp /Febuild\stance-intent\noise-probe.exe /Fobuild\stance-intent\noise-probe.obj
if errorlevel 1 exit /b 1
build\stance-intent\noise-probe.exe
