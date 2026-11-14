@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
python tools\verify_pulleys.py "C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered" build\pulley-poses.txt
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /EHsc tools\pulley_tests.cpp /Febuild\pulley_tests.exe /Fobuild\pulley_tests.obj
if errorlevel 1 exit /b 1
build\pulley_tests.exe build\pulley-poses.txt
