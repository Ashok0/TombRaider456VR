@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /Od /EHsc /Ithird_party\openvr\headers /Isrc build\arm_wobble_probe.cpp /Febuild\arm_wobble_probe.exe /Fobuild\arm_wobble_probe.obj
if errorlevel 1 exit /b 1
build\arm_wobble_probe.exe
