@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /DPLANTED_CAPTURE /std:c++17 /EHsc /Isrc build\turn-tracking\audit.cpp /Febuild\turn-tracking\audit-planted.exe /Fobuild\turn-tracking\
if errorlevel 1 exit /b 1
