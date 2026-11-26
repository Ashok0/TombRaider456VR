@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc /Ibuild/turn-pivot-response/hold-only /Isrc tools/planted_session_tests.cpp /Febuild/turn-pivot-response/hold-only/planted.exe /Fobuild/turn-pivot-response/hold-only/planted.obj
build\turn-pivot-response\hold-only\planted.exe build\stance-transitions\session.log
cl /nologo /std:c++17 /O2 /EHsc /Ibuild/turn-pivot-response/broad-only /Isrc tools/planted_session_tests.cpp /Febuild/turn-pivot-response/broad-only/planted.exe /Fobuild/turn-pivot-response/broad-only/planted.obj
build\turn-pivot-response\broad-only\planted.exe build\stance-transitions\session.log
cl /nologo /std:c++17 /O2 /EHsc /Ibuild/turn-pivot-response/continuation-only /Isrc tools/planted_session_tests.cpp /Febuild/turn-pivot-response/continuation-only/planted.exe /Fobuild/turn-pivot-response/continuation-only/planted.obj
build\turn-pivot-response\continuation-only\planted.exe build\stance-transitions\session.log