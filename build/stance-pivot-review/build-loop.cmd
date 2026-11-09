@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /D_CRT_SECURE_NO_WARNINGS /Isrc build\stance-pivot-review\replay-loop.cpp /Febuild\stance-pivot-review\loop-baseline.exe /Fobuild\stance-pivot-review\loop-baseline.obj
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /D_CRT_SECURE_NO_WARNINGS /Ibuild\stance-pivot-review\src /Isrc build\stance-pivot-review\replay-loop.cpp /Febuild\stance-pivot-review\loop-candidate.exe /Fobuild\stance-pivot-review\loop-candidate.obj
