@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc tools\cog_wheel_tests.cpp /Febuild\cog_wheel_tests.exe /Fobuild\cog_wheel_tests.obj
if errorlevel 1 exit /b 1
build\cog_wheel_tests.exe build\cog-angles.txt
