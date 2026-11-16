@echo off
set "VCI_GAME=C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered"
call tools\build_button_tests.cmd "%VCI_GAME%"
exit /b %errorlevel%
