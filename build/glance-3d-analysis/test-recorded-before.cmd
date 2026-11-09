@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\glance-3d-analysis\before456\src /Isrc build\glance-3d-analysis\test-recorded.cpp /Febuild\glance-3d-analysis\test-recorded-before.exe /Fobuild\glance-3d-analysis\test-recorded-before.obj
if errorlevel 1 exit /b 1
build\glance-3d-analysis\test-recorded-before.exe
