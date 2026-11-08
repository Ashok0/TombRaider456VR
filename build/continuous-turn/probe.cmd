@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\continuous-turn\probe.cpp /Febuild\continuous-turn\probe.exe /Fobuild\continuous-turn\probe.obj
if errorlevel 1 exit /b 1
build\continuous-turn\probe.exe
