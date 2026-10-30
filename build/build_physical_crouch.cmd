@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /O2 /Isrc tools\physical_crouch_tests.cpp /Febuild\physical_crouch_tests.exe /Fobuild\physical_crouch_tests.obj
if errorlevel 1 exit /b 1
build\physical_crouch_tests.exe
if errorlevel 1 exit /b 1
msbuild TombRaiderVR.vcxproj /p:SolutionDir=C:\dev\TombRaider456VR\ /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /p:IntDir=C:\dev\TombRaider456VR\build\physical-crouch-obj\ /p:OutDir=C:\dev\TombRaider456VR\build\physical-crouch-release\ /m /v:minimal /nologo
