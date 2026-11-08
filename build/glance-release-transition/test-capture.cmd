@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /EHsc /std:c++17 /Ibuild\glance-release-transition\before456\src /Isrc build\glance-release-transition\capture-test.cpp /Febuild\glance-release-transition\capture-before.exe /Fobuild\glance-release-transition\capture-before.obj
if errorlevel 1 exit /b 2
build\glance-release-transition\capture-before.exe
if not errorlevel 1 exit /b 3
cl /nologo /O2 /EHsc /std:c++17 /Isrc build\glance-release-transition\capture-test.cpp /Febuild\glance-release-transition\capture-after.exe /Fobuild\glance-release-transition\capture-after.obj
if errorlevel 1 exit /b 4
build\glance-release-transition\capture-after.exe
