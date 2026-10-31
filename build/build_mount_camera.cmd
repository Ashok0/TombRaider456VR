@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /O2 /W4 /Isrc tools\mount_camera_test.cpp /Febuild\mount_camera_test.exe /Fobuild\mount_camera_test.obj
if errorlevel 1 exit /b 1
build\mount_camera_test.exe
if errorlevel 1 exit /b 1
msbuild TombRaiderVR.vcxproj /p:SolutionDir=C:\dev\TombRaider456VR\ /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /p:IntDir=C:\dev\TombRaider456VR\build\mount-camera-obj\ /p:OutDir=C:\dev\TombRaider456VR\build\mount-camera-release\ /m /v:minimal /nologo
