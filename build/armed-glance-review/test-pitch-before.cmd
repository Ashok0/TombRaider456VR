@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /DLEGACY_NECK_PROJECTION /Ibuild\armed-glance-review\before456\src /Isrc build\armed-glance-review\test-pitch.cpp /Febuild\armed-glance-review\test-pitch-before.exe /Fobuild\armed-glance-review\test-pitch-before.obj
if errorlevel 1 exit /b 1
build\armed-glance-review\test-pitch-before.exe
