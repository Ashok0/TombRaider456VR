@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\ledge-hang-camera\tests mkdir build\ledge-hang-camera\tests
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\ledge_hang_camera_test.cpp tools\dynamic_bones_regression.cpp /Febuild\ledge-hang-camera\tests\ledge_hang_camera_test.exe /Fobuild\ledge-hang-camera\tests\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\ledge-hang-camera\tests\ledge_hang_camera_test.exe
