@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /D_CRT_SECURE_NO_WARNINGS /Isrc tools\body_anchor_replay.cpp /Febuild\glance-3d-analysis\body_anchor_replay.exe /Fobuild\glance-3d-analysis\body_anchor_replay.obj
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Isrc build\glance-3d-analysis\generate-replay.cpp /Febuild\glance-3d-analysis\generate-replay.exe /Fobuild\glance-3d-analysis\generate-replay.obj
if errorlevel 1 exit /b 1
build\glance-3d-analysis\generate-replay.exe > build\glance-3d-analysis\replay-fixture.log
build\glance-3d-analysis\body_anchor_replay.exe build\glance-3d-analysis\replay-fixture.log
