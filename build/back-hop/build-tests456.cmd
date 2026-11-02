@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\back-hop\test456 mkdir build\back-hop\test456
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\back_hop_test.cpp tools\dynamic_bones_regression.cpp /Febuild\back-hop\test456\back_hop_test.exe /Fobuild\back-hop\test456\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\back-hop\test456\back_hop_test.exe
