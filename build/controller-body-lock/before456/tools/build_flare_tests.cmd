@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\flare-test mkdir build\flare-test
cl /nologo /std:c++17 /Od /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc tools\flare_throw_test.cpp /Febuild\flare-test\flare456.exe /Fobuild\flare-test\flare456.obj
if errorlevel 1 exit /b 1
build\flare-test\flare456.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /Od /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DFLARE_TR123 /Isrc tools\flare_throw_test.cpp /Febuild\flare-test\flare123.exe /Fobuild\flare-test\flare123.obj
if errorlevel 1 exit /b 1
build\flare-test\flare123.exe
exit /b %errorlevel%
