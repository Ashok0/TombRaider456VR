@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /EHsc /std:c++17 /Isrc build\glance-release-transition\turn-repro.cpp /Febuild\glance-release-transition\turn-repro.exe /Fobuild\glance-release-transition\turn-repro.obj
if errorlevel 1 exit /b 1
build\glance-release-transition\turn-repro.exe
