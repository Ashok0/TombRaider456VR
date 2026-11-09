@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\motion-decomposition\src /Isrc build\extended-arm-glance\curve.cpp /Febuild\motion-decomposition\curve.exe /Fobuild\motion-decomposition\curve.obj
if errorlevel 1 exit /b 1
build\motion-decomposition\curve.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\motion-decomposition\src /Isrc build\armed-glance-review\test-pitch.cpp /Febuild\motion-decomposition\pitch.exe /Fobuild\motion-decomposition\pitch.obj
if errorlevel 1 exit /b 1
build\motion-decomposition\pitch.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\motion-decomposition\src /Isrc build\extended-arm-glance\test-extended.cpp /Febuild\motion-decomposition\extended.exe /Fobuild\motion-decomposition\extended.obj
if errorlevel 1 exit /b 1
build\motion-decomposition\extended.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\motion-decomposition\src /Isrc build\armed-glance-review\test-pitched-turn.cpp /Febuild\motion-decomposition\turn.exe /Fobuild\motion-decomposition\turn.obj
if errorlevel 1 exit /b 1
build\motion-decomposition\turn.exe
if errorlevel 1 exit /b 1
