@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist build\valve-runtime-test mkdir build\valve-runtime-test
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\switch_reach_test.cpp tools\dynamic_bones_regression.cpp /Febuild\valve-runtime-test\valve_runtime_test.exe /Fobuild\valve-runtime-test\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\valve-runtime-test\valve_runtime_test.exe
