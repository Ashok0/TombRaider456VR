@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Isrc build/slow-roomscale/inspection-diagnostic.cpp /Febuild/slow-roomscale/inspection-diagnostic.exe /Fobuild/slow-roomscale/inspection-diagnostic.obj
if errorlevel 1 exit /b 1
build\slow-roomscale\inspection-diagnostic.exe build\glance-turn-entry\stationary-glance.log
