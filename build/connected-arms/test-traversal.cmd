@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc tools\traversal_ik_test.cpp /Febuild\connected-arms\traversal_ik_test.exe /Fobuild\connected-arms\traversal_ik_test.obj
if errorlevel 1 exit /b 1
build\connected-arms\traversal_ik_test.exe
