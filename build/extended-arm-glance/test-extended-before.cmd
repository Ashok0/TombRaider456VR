@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\extended-arm-glance\before456\src /Isrc build\extended-arm-glance\test-extended.cpp /Febuild\extended-arm-glance\test-extended-before.exe /Fobuild\extended-arm-glance\test-extended-before.obj
if errorlevel 1 exit /b 1
build\extended-arm-glance\test-extended-before.exe
