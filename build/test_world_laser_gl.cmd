@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist build\world-laser-tests mkdir build\world-laser-tests
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Ithird_party\openvr\headers /Isrc tools\world_laser_gl_tests.cpp /Febuild\world-laser-tests\world_laser_gl_tests.exe /Fobuild\world-laser-tests\ /link /OPT:REF user32.lib gdi32.lib opengl32.lib
if errorlevel 1 exit /b 1
build\world-laser-tests\world_laser_gl_tests.exe
