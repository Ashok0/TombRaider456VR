@echo off
set "MONITOR_ROOT=%~dp0.."
set "MONITOR_GAME=C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered"
if not "%~1"=="" set "MONITOR_GAME=%~1"
pushd "%MONITOR_ROOT%"
python tools\verify_monitors.py "%MONITOR_GAME%"
if errorlevel 1 goto failed
call tools\build_container_tests.cmd "%MONITOR_GAME%"
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
