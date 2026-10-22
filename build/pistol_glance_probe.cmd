@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /Ithird_party\openvr\headers /Isrc build\pistol_glance_probe.cpp /Febuild\pistol_glance_probe.exe /Fobuild\pistol_glance_probe.obj
if errorlevel 1 exit /b 1
build\pistol_glance_probe.exe

