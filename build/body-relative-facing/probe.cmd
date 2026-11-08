@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\body-relative-facing\probe.cpp /Febuild\body-relative-facing\probe.exe /Fobuild\body-relative-facing\probe.obj
if errorlevel 1 exit /b 1
build\body-relative-facing\probe.exe
