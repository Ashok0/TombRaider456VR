@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers build\ledge-hang-camera\full_regression.cpp tools\dynamic_bones_regression.cpp /Febuild\ledge-hang-camera\tests\full_regression.exe /Fobuild\ledge-hang-camera\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\ledge-hang-camera\tests\full_regression.exe
