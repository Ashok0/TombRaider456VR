@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\body-anchor-continuity\probe.cpp /Febuild\body-anchor-continuity\probe.exe /Fobuild\body-anchor-continuity\probe.obj
if errorlevel 1 exit /b 1
build\body-anchor-continuity\probe.exe
