@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /Isrc build\glance-turn-entry\replay-fixture.cpp /Febuild\glance-turn-entry\replay-fixture.exe /Fobuild\glance-turn-entry\
if errorlevel 1 exit /b 1
build\glance-turn-entry\replay-fixture.exe build\glance-turn-entry\stationary-glance.log > build\glance-turn-entry\candidate-replay-input.log
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /Isrc tools\body_anchor_replay.cpp /Febuild\glance-turn-entry\body_anchor_replay.exe /Fobuild\glance-turn-entry\
if errorlevel 1 exit /b 1
build\glance-turn-entry\body_anchor_replay.exe build\glance-turn-entry\candidate-replay-input.log
