@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc tools\lowered_inspection_tests.cpp /Febuild\stance-transitions\lowered-inspection.exe /Fobuild\stance-transitions\
if errorlevel 1 exit /b 1
build\stance-transitions\lowered-inspection.exe build\glance-turn-entry\stationary-glance.log
