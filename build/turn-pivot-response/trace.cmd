@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /Ibuild/turn-pivot-response/before/src build/turn-pivot-response/rotation-replay.cpp /Febuild/turn-pivot-response/replay-baseline.exe /Fobuild/turn-pivot-response/replay-baseline.obj
if errorlevel 1 exit /b 1
build\turn-pivot-response\replay-baseline.exe build\turn-pivot-response\session.log
cl /nologo /std:c++17 /O2 /EHsc /DTRACE /Isrc build/turn-pivot-response/rotation-replay.cpp /Febuild/turn-pivot-response/replay.exe /Fobuild/turn-pivot-response/replay.obj
if errorlevel 1 exit /b 1
build\turn-pivot-response\replay.exe build\turn-pivot-response\session.log
