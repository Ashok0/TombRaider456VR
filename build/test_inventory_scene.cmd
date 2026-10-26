@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist build\inventory-scene-tests mkdir build\inventory-scene-tests
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Ithird_party\openvr\headers /Isrc tools\inventory_scene_tests.cpp /Febuild\inventory-scene-tests\inventory_scene_tests.exe /Fobuild\inventory-scene-tests\ /link /OPT:REF user32.lib gdi32.lib opengl32.lib
if errorlevel 1 exit /b 1
build\inventory-scene-tests\inventory_scene_tests.exe
