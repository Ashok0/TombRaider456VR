@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Isrc build/turn-pivot-response/inspection-diagnostic.cpp /Febuild/turn-pivot-response/inspection-diagnostic.exe /Fobuild/turn-pivot-response/inspection-diagnostic.obj
if errorlevel 1 exit /b 1
build\turn-pivot-response\inspection-diagnostic.exe build\glance-turn-entry\stationary-glance.log
