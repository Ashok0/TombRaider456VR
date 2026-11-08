@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Ibuild\tracked-glance\before456\src /Isrc build\tracked-glance\arc.cpp /Febuild\tracked-glance\arc-before.exe /Fobuild\tracked-glance\arc-before.obj
if errorlevel 1 exit /b 1
build\tracked-glance\arc-before.exe
