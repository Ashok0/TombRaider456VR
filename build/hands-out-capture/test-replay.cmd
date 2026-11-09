@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /D_CRT_SECURE_NO_WARNINGS /Isrc tools\body_anchor_replay.cpp /Febuild\hands-out-capture\body_anchor_replay.exe /Fobuild\hands-out-capture\body_anchor_replay.obj
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Isrc build\hands-out-capture\generate-replay.cpp /Febuild\hands-out-capture\generate-replay.exe /Fobuild\hands-out-capture\generate-replay.obj
if errorlevel 1 exit /b 1
build\hands-out-capture\generate-replay.exe > build\hands-out-capture\replay-fixture.log
build\hands-out-capture\body_anchor_replay.exe build\hands-out-capture\replay-fixture.log
