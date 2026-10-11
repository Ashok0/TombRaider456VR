@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /O2 /EHsc tools\switch_pose_asset_tests.cpp /Febuild\switch_pose_asset_tests.exe /Fobuild\switch_pose_asset_tests.obj
if errorlevel 1 exit /b 1
build\switch_pose_asset_tests.exe "C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered"
