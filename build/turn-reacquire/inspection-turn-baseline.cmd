@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Ibuild\turn-reacquire\before\src /Isrc tools\inspection_turn_tests.cpp /Febuild\turn-reacquire\inspection-turn-baseline.exe /Fobuild\turn-reacquire\
if errorlevel 1 exit /b 1
build\turn-reacquire\inspection-turn-baseline.exe build\turn-reacquire\session.log
