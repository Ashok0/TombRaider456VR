@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /D_CRT_SECURE_NO_WARNINGS /Isrc build\stance-pivot-review\replay-loop.cpp /Febuild\stance-pivot-review\final-replay.exe /Fobuild\stance-pivot-review\final-replay.obj
exit /b %ERRORLEVEL%
