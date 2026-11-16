@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc tools\inspection_heading_tests.cpp /Febuild\ladder-exit-facing\inspection.exe /Fobuild\ladder-exit-facing\
if errorlevel 1 exit /b 1
build\ladder-exit-facing\inspection.exe build\ladder-exit-facing\glance-input.log
