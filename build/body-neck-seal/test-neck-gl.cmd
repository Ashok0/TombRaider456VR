@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc  tools\body_neck_gl_tests.cpp src\GL.cpp /Febuild\body-neck-seal\gl-456.exe /Fobuild\body-neck-seal\456\ /link user32.lib gdi32.lib opengl32.lib
if errorlevel 1 exit /b 1
build\body-neck-seal\gl-456.exe "C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered\tomb456.exe"
