@echo off
pushd "%~dp0.."
python tools\verify_mirror.py
if errorlevel 1 goto failed
python tools\verify_mirror_faces.py
if errorlevel 1 goto failed
python tools\verify_mirror_materials.py
if errorlevel 1 goto failed
python tools\verify_native_mirror.py
if errorlevel 1 goto failed
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto failed
if not exist build\mirror-test mkdir build\mirror-test
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\mirror_test.cpp tools\dynamic_bones_regression.cpp /Febuild\mirror-test\mirror_test.exe /Fobuild\mirror-test\ /link /OPT:REF user32.lib
if errorlevel 1 goto failed
build\mirror-test\mirror_test.exe
if errorlevel 1 goto failed
cl /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\mirror_gl_test.cpp /Febuild\mirror-test\mirror_gl_test.exe /Fobuild\mirror-test\ /link user32.lib gdi32.lib opengl32.lib
if errorlevel 1 goto failed
build\mirror-test\mirror_gl_test.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
