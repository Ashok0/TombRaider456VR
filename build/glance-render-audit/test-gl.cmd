@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc tools\body_render_audit_gl_test.cpp src\GL.cpp /Febuild\glance-render-audit\tests\render_audit.exe /Fobuild\glance-render-audit\tests\ /link user32.lib gdi32.lib opengl32.lib
if errorlevel 1 exit /b 1
build\glance-render-audit\tests\render_audit.exe
