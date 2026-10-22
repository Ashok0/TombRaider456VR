@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /Od /EHsc /Ithird_party\openvr\headers /Isrc build\pistol_twist_probe.cpp /Febuild\pistol_twist_probe.exe /Fobuild\pistol_twist_probe.obj
if errorlevel 1 exit /b 1
build\pistol_twist_probe.exe

