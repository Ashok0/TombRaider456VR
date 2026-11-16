@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Isrc /Itools build/slow-roomscale/anchor-test.cpp /Febuild/slow-roomscale/anchor.exe /Fobuild/slow-roomscale/anchor.obj
if errorlevel 1 exit /b 1
build\slow-roomscale\anchor.exe 
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Isrc /Itools build/stance-transitions/swivel-test.cpp /Febuild/slow-roomscale/swivel.exe /Fobuild/slow-roomscale/swivel.obj
if errorlevel 1 exit /b 1
build\slow-roomscale\swivel.exe 
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Isrc /Itools tools/lowered_inspection_tests.cpp /Febuild/slow-roomscale/inspection.exe /Fobuild/slow-roomscale/inspection.obj
if errorlevel 1 exit /b 1
build\slow-roomscale\inspection.exe build/glance-turn-entry/stationary-glance.log
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Isrc /Itools tools/turn_continuation_tests.cpp /Febuild/slow-roomscale/continuation.exe /Fobuild/slow-roomscale/continuation.obj
if errorlevel 1 exit /b 1
build\slow-roomscale\continuation.exe 
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Isrc /Itools tools/inspection_turn_tests.cpp /Febuild/slow-roomscale/inspection-turn.exe /Fobuild/slow-roomscale/inspection-turn.obj
if errorlevel 1 exit /b 1
build\slow-roomscale\inspection-turn.exe build/turn-reacquire/session.log
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Isrc /Itools tools/opposed_stance_tests.cpp /Febuild/slow-roomscale/opposed.exe /Fobuild/slow-roomscale/opposed.obj
if errorlevel 1 exit /b 1
build\slow-roomscale\opposed.exe build/turn-tracking/session.log
if errorlevel 1 exit /b 1
