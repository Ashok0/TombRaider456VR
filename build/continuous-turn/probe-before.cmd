@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Ibuild\continuous-turn\before456 /Isrc build\continuous-turn\probe.cpp /Febuild\continuous-turn\probe-before.exe /Fobuild\continuous-turn\probe-before.obj
if errorlevel 1 exit /b 1
build\continuous-turn\probe-before.exe
