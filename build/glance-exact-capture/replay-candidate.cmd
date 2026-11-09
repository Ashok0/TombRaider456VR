@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /D_CRT_SECURE_NO_WARNINGS /Isrc build\glance-exact-capture\replay-candidate.cpp /Febuild\glance-exact-capture\replay-candidate.exe /Fobuild\glance-exact-capture\replay-candidate.obj
if errorlevel 1 exit /b 1
build\glance-exact-capture\replay-candidate.exe build\glance-exact-capture\session.log
