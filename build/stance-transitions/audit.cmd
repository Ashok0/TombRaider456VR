@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\stance-transitions\audit.cpp /Febuild\stance-transitions\audit.exe /Fobuild\stance-transitions\
if errorlevel 1 exit /b 1
build\stance-transitions\audit.exe build\stance-transitions\session.log
