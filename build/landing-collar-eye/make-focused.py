from pathlib import Path
p=Path('build/landing-collar-eye')
s=Path('tools/upjump_arms_test.cpp').read_text()
setup=s[s.index('int main() {'):s.index('    for(int which:{0,1}) for(bool fullBody:{false,true}) for(bool recovery:')]
setup=setup.replace('    TestTorsoClearance();\n','')
prefix=s[:s.index('int main() {')]
import re
prefix=re.sub(r'#include "([^\"]+)"',r'#include "../../tools/\1"',prefix)
code=prefix+setup
code+='for(int which:{0,1}) { reset(which,true);TestLandingSupportPalette(objectInfo); }\nstd::printf("PASS: %d landing/standing checks\\n",checks);VR().m_system=nullptr;\n}\n'
(p/'focused.cpp').write_text(code)
(p/'focused.cmd').write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\ncl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\\openvr\\headers build\\landing-collar-eye\\focused.cpp tools\\dynamic_bones_regression.cpp /Febuild\\landing-collar-eye\\focused.exe /Fobuild\\landing-collar-eye\\ /link /OPT:REF user32.lib\nif errorlevel 1 exit /b 1\nbuild\\landing-collar-eye\\focused.exe\n')

