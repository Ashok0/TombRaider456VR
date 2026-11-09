@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\motion-decomposition\src /Isrc build\hands-out-capture\new-recorded.cpp /Febuild\motion-decomposition\new-recorded.exe /Fobuild\motion-decomposition\new-recorded.obj
if errorlevel 1 exit /b 1
build\motion-decomposition\new-recorded.exe
