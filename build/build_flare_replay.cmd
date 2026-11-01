@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /IC:\dev\TombRaider123VR\src build\flare_replay.cpp /Febuild\flare-old-replay.exe /Fobuild\flare-old-replay.obj
if errorlevel 1 exit /b 1
build\flare-old-replay.exe build\flare-replay.csv
cl /nologo /std:c++17 /EHsc /Isrc build\flare_replay.cpp /Febuild\flare-new-replay.exe /Fobuild\flare-new-replay.obj
if errorlevel 1 exit /b 1
build\flare-new-replay.exe build\flare-replay.csv
