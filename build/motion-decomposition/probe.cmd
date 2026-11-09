@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /D_CRT_SECURE_NO_WARNINGS /Ibuild\motion-decomposition\src /Isrc build\glance-exact-capture\replay-candidate.cpp /Febuild\motion-decomposition\probe.exe /Fobuild\motion-decomposition\probe.obj
if errorlevel 1 exit /b 1
build\motion-decomposition\probe.exe build\hands-out-capture\session.log
