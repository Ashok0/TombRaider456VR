@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\torso_surface_gl_test.cpp src\GL.cpp /Febuild\torso-surface-fit\gl-456.exe /Fobuild\torso-surface-fit\456\ /link /OPT:REF user32.lib gdi32.lib opengl32.lib
if errorlevel 1 exit /b 1
build\torso-surface-fit\gl-456.exe
