@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /Ibuild\glance-release-fix\before456\src /Isrc build\glance-release-fix\replay.cpp /Febuild\glance-release-fix\replay-before.exe /Fobuild\glance-release-fix\replay-before.obj
if errorlevel 1 exit /b 1
build\glance-release-fix\replay-before.exe build\glance-release-fix\replay.csv
