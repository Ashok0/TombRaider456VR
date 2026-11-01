@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\crouch-transition-test mkdir build\crouch-transition-test
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\crouch_transition_test.cpp tools\dynamic_bones_regression.cpp /Febuild\crouch-transition-test\crouch_transition_test.exe /Fobuild\crouch-transition-test\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\crouch-transition-test\crouch_transition_test.exe
