@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers build\landing-native-animation\focused.cpp tools\dynamic_bones_regression.cpp /Febuild\landing-native-animation\focused.exe /Fobuild\landing-native-animation\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\landing-native-animation\focused.exe
