@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\connected-arms\456\baseline-tests mkdir build\connected-arms\456\baseline-tests
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Ibuild\connected-arms\before456\src /Ithird_party\openvr\headers build\connected-arms\before456\tools\upjump_arms_test.cpp build\connected-arms\before456\tools\dynamic_bones_regression.cpp /Febuild\connected-arms\456\baseline-tests\upjump_arms_test.exe /Fobuild\connected-arms\456\baseline-tests\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\connected-arms\456\baseline-tests\upjump_arms_test.exe



