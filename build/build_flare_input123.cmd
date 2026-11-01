@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /IC:\dev\TombRaider123VR\src /IC:\dev\TombRaider123VR\tests /IC:\dev\TombRaider123VR\third_party\openvr\headers build\flare123-port\tests\flare_input_test.cpp C:\dev\TombRaider123VR\src\InlineHook.cpp C:\dev\TombRaider123VR\src\Log.cpp /Febuild\flare-test\flare_input123.exe /Fobuild\flare-test\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\flare-test\flare_input123.exe
exit /b %errorlevel%
