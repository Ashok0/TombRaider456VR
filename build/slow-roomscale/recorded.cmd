@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /Ibuild\slow-roomscale\before\src build\slow-roomscale\recorded.cpp /Febuild\slow-roomscale\recorded-baseline.exe /Fobuild\slow-roomscale\recorded-baseline.obj
if errorlevel 1 exit /b 1
build\slow-roomscale\recorded-baseline.exe build\slow-roomscale\session.log
cl /nologo /std:c++17 /O2 /EHsc /DNEW_VISUAL_ANCHOR /Isrc build\slow-roomscale\recorded.cpp /Febuild\slow-roomscale\recorded.exe /Fobuild\slow-roomscale\recorded.obj
if errorlevel 1 exit /b 1
build\slow-roomscale\recorded.exe build\slow-roomscale\session.log
