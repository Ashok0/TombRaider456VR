@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Ibuild\stance-intent\before456 /Isrc build\stance-intent\side-probe.cpp /Febuild\stance-intent\side-probe-before.exe /Fobuild\stance-intent\side-probe-before.obj
if errorlevel 1 exit /b 1
build\stance-intent\side-probe-before.exe
