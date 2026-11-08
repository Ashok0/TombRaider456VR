@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\planted-glance-height\probe.cpp /Febuild\planted-glance-height\probe.exe /Fobuild\planted-glance-height\probe.obj
if errorlevel 1 exit /b 1
build\planted-glance-height\probe.exe
