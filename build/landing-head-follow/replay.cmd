@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\landing-head-follow\candidate /Isrc build\landing-head-follow\replay-debug.cpp /Febuild\landing-head-follow\replay.exe /Fobuild\landing-head-follow\replay.obj
if errorlevel 1 exit /b 1
