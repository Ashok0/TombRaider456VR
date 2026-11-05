@echo off
rem Focused interaction runner only. Never executes the full first-person suite.
call tools\build_button_tests.cmd
exit /b %errorlevel%
