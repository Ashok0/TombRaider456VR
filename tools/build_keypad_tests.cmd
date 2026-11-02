@echo off
pushd "%~dp0.."
python tools\verify_keypad.py "C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered"
if errorlevel 1 goto failed
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto failed
if not exist build\keypad-test mkdir build\keypad-test
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\keypad_test.cpp tools\dynamic_bones_regression.cpp /Febuild\keypad-test\keypad_test.exe /Fobuild\keypad-test\ /link /OPT:REF user32.lib
if errorlevel 1 goto failed
build\keypad-test\keypad_test.exe
if errorlevel 1 goto failed
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\keypad_gl_test.cpp src\GL.cpp /Febuild\keypad-test\keypad_gl_test.exe /Fobuild\keypad-test\ /link /OPT:REF user32.lib gdi32.lib opengl32.lib
if errorlevel 1 goto failed
build\keypad-test\keypad_gl_test.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
