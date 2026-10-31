@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
msbuild TombRaiderVR.vcxproj /p:SolutionDir=C:\dev\TombRaider456VR\ /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /p:IntDir=C:\dev\TombRaider456VR\build\switch-direct-obj\ /p:OutDir=C:\dev\TombRaider456VR\build\switch-direct-release\ /m /v:minimal /nologo
if errorlevel 1 exit /b 1
set "PORT=C:\dev\TombRaider456VR\build\switch-direct123"
msbuild "%PORT%\TombRaiderVR.vcxproj" /p:SolutionDir=%PORT%\ /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /p:IntDir=%PORT%\obj\ /p:OutDir=%PORT%\bin\ /m /v:minimal /nologo
