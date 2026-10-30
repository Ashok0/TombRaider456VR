@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
msbuild C:\dev\TombRaider456VR\build\jump-parity123\TombRaiderVR.vcxproj /p:SolutionDir=C:\dev\TombRaider456VR\build\jump-parity123\ /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /p:IntDir=C:\dev\TombRaider456VR\build\jump-parity123-obj\ /p:OutDir=C:\dev\TombRaider456VR\build\jump-parity123-release\ /m /v:minimal /nologo

