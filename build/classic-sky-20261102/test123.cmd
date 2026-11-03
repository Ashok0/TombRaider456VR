@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /IC:\dev\TombRaider123VR\src C:\dev\TombRaider123VR\tests\sky_selftest.cpp /Fe"%~dp0123\test\sky_test.exe" /Fo"%~dp0123\test\\" /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
"%~dp0123\test\sky_test.exe"

