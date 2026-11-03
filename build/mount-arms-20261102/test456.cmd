@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\transition_hands_gl_test.cpp src\GL.cpp /Fe"%~dp0456\test\transition_hands_gl_test.exe" /Fo"%~dp0456\test\\" /link /OPT:REF user32.lib gdi32.lib opengl32.lib
if errorlevel 1 exit /b 1
"%~dp0456\test\transition_hands_gl_test.exe"
