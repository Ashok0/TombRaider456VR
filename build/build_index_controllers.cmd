@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\index_input_test.cpp /Febuild\index_input_test.exe /Fobuild\index_input_test.obj
if errorlevel 1 exit /b 1
build\index_input_test.exe
if errorlevel 1 exit /b 1
msbuild TombRaiderVR.vcxproj /p:SolutionDir=C:\dev\TombRaider456VR\ /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /p:IntDir=C:\dev\TombRaider456VR\build\index-controllers-obj\ /p:OutDir=C:\dev\TombRaider456VR\build\index-controllers-release\ /m /v:minimal /nologo
