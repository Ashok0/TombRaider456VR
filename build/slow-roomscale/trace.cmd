@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /Isrc build\slow-roomscale\trace.cpp /Febuild\slow-roomscale\trace.exe /Fobuild\slow-roomscale\trace.obj
if errorlevel 1 exit /b 1
build\slow-roomscale\trace.exe build\slow-roomscale\session.log
