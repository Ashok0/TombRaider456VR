@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /D_CRT_SECURE_NO_WARNINGS /Ibuild\hands-out-capture\src /Isrc build\glance-exact-capture\replay-candidate.cpp /Febuild\hands-out-capture\probe.exe /Fobuild\hands-out-capture\probe.obj
if errorlevel 1 exit /b 1
build\hands-out-capture\probe.exe build\hands-out-capture\session.log
