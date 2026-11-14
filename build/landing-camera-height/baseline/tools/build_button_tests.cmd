@echo off
set "BUTTON_ROOT=%~dp0.."
set "BUTTON_GAME=C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered"
if not "%~1"=="" set "BUTTON_GAME=%~1"
pushd "%BUTTON_ROOT%"
python tools\verify_buttons.py "%BUTTON_GAME%" build\button-poses.txt
if errorlevel 1 goto failed
call tools\build_monitor_tests.cmd "%BUTTON_GAME%"
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
