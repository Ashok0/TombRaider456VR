@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\turn-tracking\audit.cpp /Febuild\turn-tracking\audit.exe /Fobuild\turn-tracking\
if errorlevel 1 exit /b 1

build\turn-tracking\audit.exe build\turn-tracking\session.log
