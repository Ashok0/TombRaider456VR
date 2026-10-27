@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist build\world-laser-regression mkdir build\world-laser-regression
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Ithird_party\openvr\headers /Isrc tools\first_person_tests.cpp tools\dynamic_bones_regression.cpp /Febuild\world-laser-regression\first_person_tests.exe /Fobuild\world-laser-regression\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\world-laser-regression\first_person_tests.exe




