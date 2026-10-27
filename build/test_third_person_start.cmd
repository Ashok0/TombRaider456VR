@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist build\third-person-start-tests mkdir build\third-person-start-tests
cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Ithird_party\openvr\headers /Isrc tools\first_person_tests.cpp tools\dynamic_bones_regression.cpp /Febuild\third-person-start-tests\first_person_tests.exe /Fobuild\third-person-start-tests\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\third-person-start-tests\first_person_tests.exe


