@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /W4 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I"C:\dev\TombRaider456VR/src" /I"C:\dev\TombRaider456VR/third_party/openvr/headers" "C:\dev\TombRaider456VR/tools/index_input_test.cpp" /Fe"C:\dev\TombRaider456VR\build\index-diagnostics-20261102\456/test/index_input_test.exe" /Fo"C:\dev\TombRaider456VR\build\index-diagnostics-20261102\456/test/index_input_test.obj"
if errorlevel 1 exit /b 1
"C:\dev\TombRaider456VR\build\index-diagnostics-20261102\456/test/index_input_test.exe"
