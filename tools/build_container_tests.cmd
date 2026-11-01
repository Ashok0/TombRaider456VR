@echo off
set "CONTAINER_ROOT=%~dp0.."
set "CONTAINER_GAME=C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered"
if not "%~1"=="" set "CONTAINER_GAME=%~1"
pushd "%CONTAINER_ROOT%"
python tools\verify_containers.py "%CONTAINER_GAME%" build\container-poses.bin
if errorlevel 1 goto failed
python tools\verify_vci_levers.py "%CONTAINER_GAME%" build\vci-lever-poses.bin
if errorlevel 1 goto failed
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto failed
if not exist build\container-runtime-test mkdir build\container-runtime-test
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\openvr\headers tools\switch_reach_test.cpp tools\dynamic_bones_regression.cpp /Febuild\container-runtime-test\container_runtime_test.exe /Fobuild\container-runtime-test\ /link /OPT:REF user32.lib
if errorlevel 1 goto failed
build\container-runtime-test\container_runtime_test.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
