@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\stance-pivot-review\src /Isrc build\extended-arm-glance\curve.cpp /Febuild\stance-pivot-review\curve.exe /Fobuild\stance-pivot-review\curve.obj
if errorlevel 1 exit /b 1
build\stance-pivot-review\curve.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\stance-pivot-review\src /Isrc build\armed-glance-review\test-pitch.cpp /Febuild\stance-pivot-review\pitch.exe /Fobuild\stance-pivot-review\pitch.obj
if errorlevel 1 exit /b 1
build\stance-pivot-review\pitch.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\stance-pivot-review\src /Isrc build\extended-arm-glance\test-extended.cpp /Febuild\stance-pivot-review\extended.exe /Fobuild\stance-pivot-review\extended.obj
if errorlevel 1 exit /b 1
build\stance-pivot-review\extended.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\stance-pivot-review\src /Isrc build\armed-glance-review\test-pitched-turn.cpp /Febuild\stance-pivot-review\turn.exe /Fobuild\stance-pivot-review\turn.obj
if errorlevel 1 exit /b 1
build\stance-pivot-review\turn.exe
if errorlevel 1 exit /b 1
