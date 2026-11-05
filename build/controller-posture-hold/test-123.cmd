@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
call "C:\dev\TombRaider456VR\build\controller-posture-hold\123-work\tests\build_jump_arms.cmd"
