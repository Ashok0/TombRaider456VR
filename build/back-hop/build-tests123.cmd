@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /IC:\dev\TombRaider123VR\src /IC:\dev\TombRaider123VR\third_party\openvr\headers C:\dev\TombRaider123VR\tests\back_hop_test.cpp C:\dev\TombRaider123VR\src\InlineHook.cpp C:\dev\TombRaider123VR\src\Log.cpp /Febuild\back-hop\test123\back_hop_test.exe /Fobuild\back-hop\test123\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\back-hop\test123\back_hop_test.exe
