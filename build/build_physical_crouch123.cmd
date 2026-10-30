@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /O2 /Ibuild\physical-crouch123\src build\physical-crouch123\tests\physical_crouch_tests.cpp /Febuild\physical_crouch123_tests.exe /Fobuild\physical_crouch123_tests.obj
if errorlevel 1 exit /b 1
build\physical_crouch123_tests.exe
if errorlevel 1 exit /b 1
msbuild C:\dev\TombRaider456VR\build\physical-crouch123\TombRaiderVR.vcxproj /p:SolutionDir=C:\dev\TombRaider456VR\build\physical-crouch123\ /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /p:IntDir=C:\dev\TombRaider456VR\build\physical-crouch123-obj\ /p:OutDir=C:\dev\TombRaider456VR\build\physical-crouch123-release\ /m /v:minimal /nologo
