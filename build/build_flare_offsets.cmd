@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers build\flare_offsets.cpp /Febuild\flare_offsets.exe /Fobuild\flare_offsets.obj
if errorlevel 1 exit /b 1
build\flare_offsets.exe
