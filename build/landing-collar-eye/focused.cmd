@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers build\landing-collar-eye\focused.cpp tools\dynamic_bones_regression.cpp /Febuild\landing-collar-eye\focused.exe /Fobuild\landing-collar-eye\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\landing-collar-eye\focused.exe
