@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /std:c++17 /EHsc /Isrc /Itools build/turn-pivot-response/anchor-test.cpp /Febuild/turn-pivot-response/anchor.exe /Fobuild/turn-pivot-response/anchor.obj
if errorlevel 1 exit /b 1
build\turn-pivot-response\anchor.exe 
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Isrc /Itools build/stance-transitions/swivel-test.cpp /Febuild/turn-pivot-response/swivel.exe /Fobuild/turn-pivot-response/swivel.obj
if errorlevel 1 exit /b 1
build\turn-pivot-response\swivel.exe 
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Isrc /Itools tools/lowered_inspection_tests.cpp /Febuild/turn-pivot-response/inspection.exe /Fobuild/turn-pivot-response/inspection.obj
if errorlevel 1 exit /b 1
build\turn-pivot-response\inspection.exe build/glance-turn-entry/stationary-glance.log
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Isrc /Itools tools/turn_continuation_tests.cpp /Febuild/turn-pivot-response/continuation.exe /Fobuild/turn-pivot-response/continuation.obj
if errorlevel 1 exit /b 1
build\turn-pivot-response\continuation.exe 
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Isrc /Itools tools/inspection_turn_tests.cpp /Febuild/turn-pivot-response/inspection-turn.exe /Fobuild/turn-pivot-response/inspection-turn.obj
if errorlevel 1 exit /b 1
build\turn-pivot-response\inspection-turn.exe build/turn-reacquire/session.log
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++17 /EHsc /Isrc /Itools tools/opposed_stance_tests.cpp /Febuild/turn-pivot-response/opposed.exe /Fobuild/turn-pivot-response/opposed.obj
if errorlevel 1 exit /b 1
build\turn-pivot-response\opposed.exe build/turn-tracking/session.log
if errorlevel 1 exit /b 1
