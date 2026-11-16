@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc tools\inspection_turn_tests.cpp /Febuild\turn-reacquire\inspection-turn.exe /Fobuild\turn-reacquire\
if errorlevel 1 exit /b 1
build\turn-reacquire\inspection-turn.exe build\turn-reacquire\session.log
