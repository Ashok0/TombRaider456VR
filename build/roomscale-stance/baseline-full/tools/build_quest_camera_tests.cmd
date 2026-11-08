@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\quest-camera-test mkdir build\quest-camera-test
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\quest_camera_test.cpp tools\dynamic_bones_regression.cpp /Febuild\quest-camera-test\quest_camera_test.exe /Fobuild\quest-camera-test\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\quest-camera-test\quest_camera_test.exe
