@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /D_CRT_SECURE_NO_WARNINGS /Ibuild\stance-pivot-review\src /Isrc build\glance-exact-capture\replay-candidate.cpp /Febuild\stance-pivot-review\probe.exe /Fobuild\stance-pivot-review\probe.obj
if errorlevel 1 exit /b 1
build\stance-pivot-review\probe.exe build\hands-out-capture\session.log
