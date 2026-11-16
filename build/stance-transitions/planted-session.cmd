@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc tools\planted_session_tests.cpp /Febuild\stance-transitions\planted-session.exe /Fobuild\stance-transitions\
if errorlevel 1 exit /b 1
build\stance-transitions\planted-session.exe build\stance-transitions\session.log
