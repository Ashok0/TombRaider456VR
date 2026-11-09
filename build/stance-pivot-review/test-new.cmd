@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\stance-pivot-review\src /Isrc build\hands-out-capture\new-recorded.cpp /Febuild\stance-pivot-review\new-recorded.exe /Fobuild\stance-pivot-review\new-recorded.obj
if errorlevel 1 exit /b 1
build\stance-pivot-review\new-recorded.exe
