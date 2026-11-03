@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /I"C:\dev\TombRaider456VR/src" /I"C:\dev\TombRaider456VR/third_party/openvr/headers" "C:\dev\TombRaider456VR/tools/hybrid_controls_config_test.cpp" "C:\dev\TombRaider456VR/src/Config.cpp" "C:\dev\TombRaider456VR/src/Log.cpp" /Fe"C:\dev\TombRaider456VR\build\hybrid-controls-20261102\456/config-tests/test.exe" /Fo"C:\dev\TombRaider456VR\build\hybrid-controls-20261102\456/config-tests/" /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
"C:\dev\TombRaider456VR\build\hybrid-controls-20261102\456/config-tests/test.exe" "C:\dev\TombRaider456VR\build\hybrid-controls-20261102\456/config-tests/test.ini"
