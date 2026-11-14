@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\landing_camera_test.cpp tools\dynamic_bones_regression.cpp /Febuild\landing-camera-height\camera-tests.exe /Fobuild\landing-camera-height\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\landing-camera-height\camera-tests.exe
