@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\hands-out-capture\src /Isrc build\extended-arm-glance\curve.cpp /Febuild\hands-out-capture\curve.exe /Fobuild\hands-out-capture\curve.obj
if errorlevel 1 exit /b 1
build\hands-out-capture\curve.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\hands-out-capture\src /Isrc build\armed-glance-review\test-pitch.cpp /Febuild\hands-out-capture\pitch.exe /Fobuild\hands-out-capture\pitch.obj
if errorlevel 1 exit /b 1
build\hands-out-capture\pitch.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\hands-out-capture\src /Isrc build\extended-arm-glance\test-extended.cpp /Febuild\hands-out-capture\extended.exe /Fobuild\hands-out-capture\extended.obj
if errorlevel 1 exit /b 1
build\hands-out-capture\extended.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Ibuild\hands-out-capture\src /Isrc build\armed-glance-review\test-pitched-turn.cpp /Febuild\hands-out-capture\turn.exe /Fobuild\hands-out-capture\turn.obj
if errorlevel 1 exit /b 1
build\hands-out-capture\turn.exe
if errorlevel 1 exit /b 1
