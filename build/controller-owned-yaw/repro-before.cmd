@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Ibuild\controller-owned-yaw\before456 /Isrc build\controller-owned-yaw\repro-before.cpp /Febuild\controller-owned-yaw\repro-before.exe /Fobuild\controller-owned-yaw\repro-before.obj
if errorlevel 1 exit /b 1
build\controller-owned-yaw\repro-before.exe
