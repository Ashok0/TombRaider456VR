@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\glance-3d-analysis\src /Isrc build\extended-arm-glance\curve.cpp /Febuild\glance-3d-analysis\curve.exe /Fobuild\glance-3d-analysis\curve.obj
if errorlevel 1 exit /b 1
build\glance-3d-analysis\curve.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\glance-3d-analysis\src /Isrc build\armed-glance-review\test-pitch.cpp /Febuild\glance-3d-analysis\pitch.exe /Fobuild\glance-3d-analysis\pitch.obj
if errorlevel 1 exit /b 1
build\glance-3d-analysis\pitch.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\glance-3d-analysis\src /Isrc build\extended-arm-glance\test-extended.cpp /Febuild\glance-3d-analysis\extended.exe /Fobuild\glance-3d-analysis\extended.obj
if errorlevel 1 exit /b 1
build\glance-3d-analysis\extended.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\glance-3d-analysis\src /Isrc build\armed-glance-review\test-pitched-turn.cpp /Febuild\glance-3d-analysis\turn.exe /Fobuild\glance-3d-analysis\turn.obj
if errorlevel 1 exit /b 1
build\glance-3d-analysis\turn.exe
if errorlevel 1 exit /b 1
