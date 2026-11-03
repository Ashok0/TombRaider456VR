@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
msbuild "C:\dev\TombRaider456VR/TombRaiderVR.vcxproj" /p:SolutionDir=C:\dev\TombRaider456VR/ /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /p:IntDir=C:\dev\TombRaider456VR\build\mount-arms-20261102\456/obj/ /p:OutDir=C:\dev\TombRaider456VR\build\mount-arms-20261102\456/bin/ /m /v:minimal /nologo

