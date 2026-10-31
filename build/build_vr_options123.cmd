@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "PORT=C:\dev\TombRaider456VR\build\vr-options123"
msbuild "%PORT%\TombRaiderVR.vcxproj" /p:SolutionDir=%PORT%\ /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /p:IntDir=%PORT%\obj\ /p:OutDir=%PORT%\bin\ /m /v:minimal /nologo



