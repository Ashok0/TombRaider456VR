@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "PORT=C:\dev\TombRaider456VR\build\switch-hand-reach123"
if not exist "%PORT%\obj-test" mkdir "%PORT%\obj-test"
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I"%PORT%\src" /I"%PORT%\third_party\openvr\headers" "%PORT%\tests\switch_reach_selftest.cpp" "%PORT%\src\InlineHook.cpp" "%PORT%\src\Log.cpp" /Fe"%PORT%\switch_reach_test.exe" /Fo"%PORT%\obj-test\\" /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
"%PORT%\switch_reach_test.exe"
if errorlevel 1 exit /b 1
msbuild "%PORT%\TombRaiderVR.vcxproj" /p:SolutionDir=%PORT%\ /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /p:IntDir=%PORT%\obj\ /p:OutDir=%PORT%\bin\ /m /v:minimal /nologo
