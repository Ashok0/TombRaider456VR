@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\planted-turn\intent_probe.cpp /Febuild\planted-turn\intent_probe.exe /Fobuild\planted-turn\intent_probe.obj
if errorlevel 1 exit /b 1
build\planted-turn\intent_probe.exe
