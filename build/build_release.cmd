@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
msbuild TombRaiderVR.sln /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /m /v:minimal /nologo
