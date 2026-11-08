@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\controller-owned-yaw\probe.cpp /Febuild\controller-owned-yaw\probe.exe /Fobuild\controller-owned-yaw\probe.obj
if errorlevel 1 exit /b 1
build\controller-owned-yaw\probe.exe
