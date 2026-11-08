@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\tracked-glance\probe.cpp /Febuild\tracked-glance\probe.exe /Fobuild\tracked-glance\probe.obj
if errorlevel 1 exit /b 1
build\tracked-glance\probe.exe
