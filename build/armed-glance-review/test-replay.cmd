@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /D_CRT_SECURE_NO_WARNINGS /Isrc tools\body_anchor_replay.cpp /Febuild\armed-glance-review\body_anchor_replay.exe /Fobuild\armed-glance-review\body_anchor_replay.obj
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Isrc build\armed-glance-review\generate-replay.cpp /Febuild\armed-glance-review\generate-replay.exe /Fobuild\armed-glance-review\generate-replay.obj
if errorlevel 1 exit /b 1
build\armed-glance-review\generate-replay.exe > build\armed-glance-review\replay-fixture.log
build\armed-glance-review\body_anchor_replay.exe build\armed-glance-review\replay-fixture.log
