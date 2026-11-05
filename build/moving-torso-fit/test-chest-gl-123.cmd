@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DTR123_TORSO_TEST /Ibuild\moving-torso-fit\123-work\src /Ithird_party\openvr\headers tools\chest_region_regression.cpp src\GL.cpp /Febuild\moving-torso-fit\chest-gl-123.exe /Fobuild\moving-torso-fit\123\ /link /OPT:REF user32.lib gdi32.lib opengl32.lib
if errorlevel 1 exit /b 1
build\moving-torso-fit\chest-gl-123.exe
