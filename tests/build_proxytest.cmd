@echo off
REM Builds and runs the winmm proxy test against the Release build output.
REM Requires the solution to have been built first.
REM
REM No parenthesised blocks: %ProgramFiles(x86)% contains parentheses and breaks
REM cmd's block parsing even when quoted.
setlocal

where cl.exe >nul 2>&1
if not errorlevel 1 goto have_cl

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_vs
"%VSWHERE%" -latest -products * -property installationPath > "%TEMP%\_trvr_vspath2.txt" 2>nul
set "VSPATH="
set /p VSPATH=<"%TEMP%\_trvr_vspath2.txt"
del "%TEMP%\_trvr_vspath2.txt" >nul 2>&1
if not defined VSPATH goto no_vs
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl.exe >nul 2>&1
if errorlevel 1 goto no_vs

:have_cl
set "ROOT=%~dp0.."
set "REL=%ROOT%\build\x64\Release"
if not exist "%REL%\winmm.dll" goto no_build

REM Stage a folder that looks like the game folder.
set "STAGE=%ROOT%\build\proxytest"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"
copy /y "%REL%\winmm.dll" "%STAGE%" >nul
copy /y "%REL%\TombRaiderVR.dll" "%STAGE%" >nul
copy /y "%ROOT%\TombRaiderVR.ini" "%STAGE%" >nul

pushd "%STAGE%"
cl /nologo /std:c++17 /EHsc /W4 /MT /O2 /DWIN32_LEAN_AND_MEAN "%ROOT%\tests\proxytest.cpp" /Fe:proxytest.exe >nul
if errorlevel 1 goto build_failed

.\proxytest.exe "%STAGE%"
set RC=%ERRORLEVEL%
echo.
echo --- TombRaiderVR-proxy.log ---
if exist TombRaiderVR-proxy.log type TombRaiderVR-proxy.log
popd
exit /b %RC%

:build_failed
popd
echo Build failed.
exit /b 1

:no_build
echo Build the solution first: msbuild TombRaiderVR.sln /p:Configuration=Release /p:Platform=x64
exit /b 1

:no_vs
echo Could not locate a Visual Studio x64 toolchain. Run from a developer prompt.
exit /b 1
