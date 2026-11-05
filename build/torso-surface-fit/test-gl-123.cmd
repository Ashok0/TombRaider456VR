@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /Gy /EHsc /DTR123_TORSO_TEST /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Ibuild\torso-surface-fit\123-work\src /Ithird_party\openvr\headers tools\torso_surface_gl_test.cpp build\torso-surface-fit\123-work\src\GL.cpp /Febuild\torso-surface-fit\gl-123.exe /Fobuild\torso-surface-fit\123\ /link /OPT:REF user32.lib gdi32.lib opengl32.lib
if errorlevel 1 exit /b 1
build\torso-surface-fit\gl-123.exe
