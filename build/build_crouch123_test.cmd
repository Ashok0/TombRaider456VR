@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\crouch123-test mkdir build\crouch123-test
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /IC:\dev\TombRaider123VR\src /IC:\dev\TombRaider123VR\tests /IC:\dev\TombRaider123VR\third_party\openvr\headers build\crouch123-port\tests\crouch_transition_test.cpp C:\dev\TombRaider123VR\src\InlineHook.cpp C:\dev\TombRaider123VR\src\Log.cpp /Febuild\crouch123-test\crouch_transition_test.exe /Fobuild\crouch123-test\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\crouch123-test\crouch_transition_test.exe
