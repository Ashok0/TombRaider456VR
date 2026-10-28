@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\dev\TombRaider123VR
msbuild TombRaiderVR.vcxproj /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /p:SolutionDir=C:\dev\TombRaider123VR\ /p:IntDir=C:\dev\TombRaider123VR\build\camera-arm-port\obj\ /p:OutDir=C:\dev\TombRaider123VR\build\camera-arm-port\release\ /m /v:minimal /nologo
