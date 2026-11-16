@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\glance-turn-entry\motion-audit.cpp /Febuild\glance-turn-entry\motion-audit.exe /Fobuild\glance-turn-entry\
if errorlevel 1 exit /b 1
build\glance-turn-entry\motion-audit.exe
