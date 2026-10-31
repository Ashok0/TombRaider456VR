from pathlib import Path
import shutil
root=Path('C:/dev/TombRaider456VR');stage=root/'build/vr-options123'
shared=['VRMenu.cpp','VRMenu.h','VRMenuModel.h','VRMenuSettings.h','VRMenuSettings.inl','VRNativeMenu.cpp','VRNativeMenuModel.h','VRNativeMenuExtra.inl']
for name in shared:shutil.copy2(root/'src'/name,stage/'src'/name)
for name in ['vr_menu_tests.cpp','vr_native_menu_tests.cpp']:shutil.copy2(root/'tools'/name,stage/'tools'/name)
for d,entry in [(root,'Open **Options > VR Options** from the title screen.'),(stage,'Select **VR Options** in the title inventory ring.')]:
 p=d/'README.md';s=p.read_text(encoding='utf-8');a=s.index('## VR OPTIONS');b=s.index('\n## ',a+3)
 s=s[:a]+'''## VR Options

'''+entry+''' The page is drawn by the
game's native menu renderer and contains only **Controller Type**:
**Meta Touch** or **Valve Index**. Left/right or Select changes the type;
Back returns to the parent menu. There is no popup or separate VR overlay.

Changes save automatically to `ValveIndexControllers` in the installed INI
and apply after restarting the game. Other INI settings remain unchanged;
the previous INI is backed up as `TombRaiderVR.ini.vr-options.bak`.
Index mode uses the existing `TombRaiderVRBindings` files beside the DLL.

'''+s[b:];p.write_text(s,encoding='utf-8')
 p=d/'TombRaiderVR.vcxproj';s=p.read_text()
 if 'VRNativeMenuExtra.inl' not in s:s=s.replace('<ClInclude Include="src\\VRNativeMenuAddresses.inl" />','<ClInclude Include="src\\VRNativeMenuAddresses.inl" />\n    <ClInclude Include="src\\VRNativeMenuExtra.inl" />')
 s=s.replace('gdi32.lib;','')
 p.write_text(s)
 p=d/'build/test_vr_options.cmd';s=p.read_text().replace(' gdi32.lib','');p.write_text(s)
