@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\hands-out-capture\src /Isrc build\hands-out-capture\new-recorded.cpp /Febuild\hands-out-capture\new-recorded.exe /Fobuild\hands-out-capture\new-recorded.obj
if errorlevel 1 exit /b 1
build\hands-out-capture\new-recorded.exe
