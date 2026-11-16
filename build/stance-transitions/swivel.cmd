@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc /Itools build\stance-transitions\swivel-test.cpp /Febuild\stance-transitions\swivel-test.exe /Fobuild\stance-transitions\
if errorlevel 1 exit /b 1
build\stance-transitions\swivel-test.exe
