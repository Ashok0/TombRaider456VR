@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\transition-hands-test-obj mkdir build\transition-hands-test-obj
cl /nologo /std:c++17 /O2 /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\transition_hands_gl_test.cpp src\GL.cpp /Febuild\transition_hands_gl_test.exe /Fobuild\transition-hands-test-obj\ /link /OPT:REF user32.lib gdi32.lib opengl32.lib
if errorlevel 1 exit /b 1
build\transition_hands_gl_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /O2 /W4 /Isrc tools\traversal_ik_test.cpp /Febuild\traversal_ik_test.exe /Fobuild\traversal_ik_test.obj
if errorlevel 1 exit /b 1
build\traversal_ik_test.exe
if errorlevel 1 exit /b 1
msbuild TombRaiderVR.vcxproj /p:SolutionDir=C:\dev\TombRaider456VR\ /p:Configuration=Release /p:Platform=x64 /p:DeployToGame=false /p:TrackFileAccess=false /p:IntDir=C:\dev\TombRaider456VR\build\transition-hands-obj\ /p:OutDir=C:\dev\TombRaider456VR\build\transition-hands-release\ /m /v:minimal /nologo
