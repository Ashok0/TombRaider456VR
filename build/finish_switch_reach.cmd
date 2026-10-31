@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\dynamic_bones_regression.cpp build\switch-hand-reach-test\switch_reach_test.obj /Febuild\switch-hand-reach-test\switch_reach_test.exe /Fobuild\switch-hand-reach-test\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\switch-hand-reach-test\switch_reach_test.exe
if errorlevel 1 exit /b 1
msbuild TombRaiderVR.vcxproj /p:SolutionDir=C:\dev\TombRaider456VR\ /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /p:IntDir=C:\dev\TombRaider456VR\build\switch-hand-reach-obj\ /p:OutDir=C:\dev\TombRaider456VR\build\switch-hand-reach-release\ /m /v:minimal /nologo
