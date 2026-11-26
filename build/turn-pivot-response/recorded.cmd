@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /Ibuild\turn-pivot-response\before\src build\turn-pivot-response\recorded.cpp /Febuild\turn-pivot-response\recorded-baseline.exe /Fobuild\turn-pivot-response\recorded-baseline.obj
if errorlevel 1 exit /b 1
build\turn-pivot-response\recorded-baseline.exe build\turn-pivot-response\session.log
cl /nologo /std:c++17 /O2 /EHsc /DNEW_VISUAL_ANCHOR /Isrc build\turn-pivot-response\recorded.cpp /Febuild\turn-pivot-response\recorded.exe /Fobuild\turn-pivot-response\recorded.obj
if errorlevel 1 exit /b 1
build\turn-pivot-response\recorded.exe build\turn-pivot-response\session.log
