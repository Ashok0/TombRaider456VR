@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Ithird_party\openvr\headers /Isrc /Itools build\fp-wall-eye-before-tests.cpp tools\dynamic_bones_regression.cpp /Febuild\fp-wall-eye-before-tests.exe /Fobuild\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\fp-wall-eye-before-tests.exe