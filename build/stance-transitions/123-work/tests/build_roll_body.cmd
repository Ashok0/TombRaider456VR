@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "ROLL_ROOT=%~dp0.."
if not exist "%ROLL_ROOT%\build\roll-body-frame" mkdir "%ROLL_ROOT%\build\roll-body-frame"
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I"%ROLL_ROOT%\src" /I"%ROLL_ROOT%\third_party\openvr\headers" "%ROLL_ROOT%\tests\roll_body_selftest.cpp" "%ROLL_ROOT%\src\InlineHook.cpp" "%ROLL_ROOT%\src\Log.cpp" /Fe"%ROLL_ROOT%\build\roll-body-frame\roll_body_test.exe" /Fo"%ROLL_ROOT%\build\roll-body-frame\\" /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
"%ROLL_ROOT%\build\roll-body-frame\roll_body_test.exe"
