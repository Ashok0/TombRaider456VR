from pathlib import Path
import shutil
root=Path('C:/dev/TombRaider456VR');stage=root/'build/vr-options123'
shared=['VRMenu.cpp','VRMenu.h','VRMenuModel.h','VRNativeMenu.cpp','VRNativeMenu.h','VRNativeMenuModel.h','VRNativeMenuAddresses.inl']
for name in shared:shutil.copy2(root/'src'/name,stage/'src'/name)
# Both copies have the same verified inline-hook implementation.
for name in ['InlineHook.h','InlineHook.cpp']:shutil.copy2(root/'src'/name,stage/'src'/name)
for name in ['vr_menu_tests.cpp','vr_native_menu_tests.cpp']:shutil.copy2(root/'tools'/name,stage/'tools'/name)
for directory in [root,stage]:
 p=directory/'TombRaiderVR.vcxproj';s=p.read_text()
 if 'src\\VRNativeMenu.cpp' not in s:
  s=s.replace('<ClCompile Include="src\\VRMenu.cpp" />','<ClCompile Include="src\\VRMenu.cpp" />\n    <ClCompile Include="src\\VRNativeMenu.cpp" />')
  s=s.replace('<ClInclude Include="src\\VRMenu.h" />','<ClInclude Include="src\\VRMenu.h" />\n    <ClInclude Include="src\\VRNativeMenu.h" />\n    <ClInclude Include="src\\VRNativeMenuModel.h" />\n    <ClInclude Include="src\\VRNativeMenuAddresses.inl" />')
 p.write_text(s)
 p=directory/'build/test_vr_options.cmd';s=p.read_text()
 if 'vr_native_menu_tests.cpp' not in s:
  s=s.replace('pushd build\\vr-menu-test','''cl /nologo /std:c++17 /Od /Gy /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Ithird_party\\openvr\\headers tools\\vr_native_menu_tests.cpp src\\InlineHook.cpp src\\Log.cpp /Febuild\\vr-menu-test\\vr_native_menu_tests.exe /Fobuild\\vr-menu-test\\ /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
build\\vr-menu-test\\vr_native_menu_tests.exe
if errorlevel 1 exit /b 1
pushd build\\vr-menu-test''')
 p.write_text(s)
