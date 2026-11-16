@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /Isrc tools\slow_roomscale_tests.cpp /Febuild\slow-roomscale\slow.exe /Fobuild\slow-roomscale\slow.obj
if errorlevel 1 exit /b 1
build\slow-roomscale\slow.exe 
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /EHsc /Isrc tools\planted_session_tests.cpp /Febuild\slow-roomscale\planted.exe /Fobuild\slow-roomscale\planted.obj
if errorlevel 1 exit /b 1
build\slow-roomscale\planted.exe build\stance-transitions\session.log
if errorlevel 1 exit /b 1
