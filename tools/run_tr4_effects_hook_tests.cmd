@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /Isrc tools\tr4_effects_hook_tests.cpp /Febuild\tr4_effects_hook_tests.exe /Fobuild\ /link opengl32.lib
if errorlevel 1 exit /b 1
build\tr4_effects_hook_tests.exe
