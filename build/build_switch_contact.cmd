@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\switch-contact-test mkdir build\switch-contact-test
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\switch_reach_test.cpp tools\dynamic_bones_regression.cpp /Febuild\switch-contact-test\switch_reach_test.exe /Fobuild\switch-contact-test\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\switch-contact-test\switch_reach_test.exe
if errorlevel 1 exit /b 1
msbuild TombRaiderVR.vcxproj /p:SolutionDir=C:\dev\TombRaider456VR\ /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /p:IntDir=C:\dev\TombRaider456VR\build\switch-contact-obj\ /p:OutDir=C:\dev\TombRaider456VR\build\switch-contact-release\ /m /v:minimal /nologo





