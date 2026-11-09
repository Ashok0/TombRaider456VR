@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /D_CRT_SECURE_NO_WARNINGS /Ibuild\glance-exact-capture\before456\src /Isrc build\glance-exact-capture\replay-candidate.cpp /Febuild\glance-3d-analysis\probe-before.exe /Fobuild\glance-3d-analysis\probe-before.obj
if errorlevel 1 exit /b 1
build\glance-3d-analysis\probe-before.exe build\glance-exact-capture\session.log
