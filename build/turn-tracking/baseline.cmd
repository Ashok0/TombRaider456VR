@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /DBASELINE_RECORDING /Ibuild\turn-tracking\before\src /Isrc tools\opposed_stance_tests.cpp /Febuild\turn-tracking\opposed-baseline.exe /Fobuild\turn-tracking\
if errorlevel 1 exit /b 1
build\turn-tracking\opposed-baseline.exe build\turn-tracking\session.log
