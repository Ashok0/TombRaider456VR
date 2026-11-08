@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist build\roomscale-stance\full mkdir build\roomscale-stance\full
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers build\roomscale-stance\full_tests.cpp tools\dynamic_bones_regression.cpp /Febuild\roomscale-stance\full\first_person_tests.exe /Fobuild\roomscale-stance\full\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\roomscale-stance\full\first_person_tests.exe
