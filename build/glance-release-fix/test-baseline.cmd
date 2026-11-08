@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\glance-release-fix\baseline-tests mkdir build\glance-release-fix\baseline-tests
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers build\glance-release-fix\baseline\tools\upjump_arms_test.cpp build\glance-release-fix\baseline\tools\dynamic_bones_regression.cpp /Febuild\glance-release-fix\baseline-tests\upjump_arms_test.exe /Fobuild\glance-release-fix\baseline-tests\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\glance-release-fix\baseline-tests\upjump_arms_test.exe




