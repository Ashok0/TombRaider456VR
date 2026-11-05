@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\ground-queue-test mkdir build\ground-queue-test
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\ground_queue_test.cpp tools\dynamic_bones_regression.cpp /Febuild\ground-queue-test\ground_queue_test.exe /Fobuild\ground-queue-test\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\ground-queue-test\ground_queue_test.exe
