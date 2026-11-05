@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX  /Isrc /Ithird_party\openvr\headers build\moving-torso-fit\old-chest-regression.cpp src\GL.cpp /Febuild\moving-torso-fit\old-chest-gl-456.exe /Fobuild\moving-torso-fit\456\ /link /OPT:REF user32.lib gdi32.lib opengl32.lib
if errorlevel 1 exit /b 1
build\moving-torso-fit\old-chest-gl-456.exe
