@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\tracked-glance\slow-continuous.cpp /Febuild\tracked-glance\slow-continuous.exe /Fobuild\tracked-glance\slow-continuous.obj
if errorlevel 1 exit /b 1
build\tracked-glance\slow-continuous.exe
