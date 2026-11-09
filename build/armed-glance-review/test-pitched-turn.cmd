@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Isrc build\armed-glance-review\test-pitched-turn.cpp /Febuild\armed-glance-review\test-pitched-turn.exe /Fobuild\armed-glance-review\test-pitched-turn.obj
if errorlevel 1 exit /b 1
build\armed-glance-review\test-pitched-turn.exe
