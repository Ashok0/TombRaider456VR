@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /Isrc build\glance-release-fix\replay.cpp /Febuild\glance-release-fix\replay.exe /Fobuild\glance-release-fix\replay.obj
if errorlevel 1 exit /b 1
build\glance-release-fix\replay.exe build\glance-release-fix\replay.csv
