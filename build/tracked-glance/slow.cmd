@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\tracked-glance\slow.cpp /Febuild\tracked-glance\slow.exe /Fobuild\tracked-glance\slow.obj
if errorlevel 1 exit /b 1
build\tracked-glance\slow.exe
