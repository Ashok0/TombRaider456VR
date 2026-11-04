@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc  tools\body_clearance_gl_tests.cpp src\GL.cpp /Febuild\torso-clearance\gl-456.exe /Fobuild\torso-clearance\456\ /link user32.lib gdi32.lib opengl32.lib
if errorlevel 1 exit /b 1
build\torso-clearance\gl-456.exe "C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered\tomb456.exe"
