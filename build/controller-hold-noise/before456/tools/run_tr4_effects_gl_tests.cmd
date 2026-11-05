@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc tools\tr4_effects_gl_tests.cpp src\GL.cpp /Febuild\tr4_effects_gl_tests.exe /Fobuild\ /link user32.lib gdi32.lib opengl32.lib
if errorlevel 1 exit /b 1
build\tr4_effects_gl_tests.exe "C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered"
