from pathlib import Path
import shutil
root=Path('C:/dev/TombRaider456VR');stage=root/'build/vr-options123'
for name in ['VRMenuModel.h','VRMenuSettings.h','VRMenuSettings.inl','VRNativeMenu.cpp']:
 shutil.copy2(root/'src'/name,stage/'src'/name)
for name in ['vr_menu_tests.cpp','vr_native_menu_tests.cpp']:
 shutil.copy2(root/'tools'/name,stage/'tools'/name)
for repo in [root,stage]:
 p=repo/'README.md';s=p.read_text(encoding='utf-8');start=s.index('## VR Options');end=s.index('\n## ',start+3)
 entry='Open **Options > VR Options** from the title screen.' if repo==root else 'Select **VR Options** in the title inventory ring.'
 s=s[:start]+'''## VR Options

'''+entry+''' This uses the game's native menu, with no popup.
Use Up/Down to select a row, Left/Right or Select to change it, and Back to return.

- **Controller Type:** Meta Touch or Valve Index. Changes apply after restarting.
- **Color Scheme:** Dark (default) or Brighter. Changes apply immediately in the headset.
  Dark retains the original VR presentation. Brighter lifts midtones with display
  encoding to give a look closer to the brighter flat game; an exact monitor match
  depends on the headset and display. It does not change game lighting or the desktop mirror.

Changes save automatically to `ValveIndexControllers` and `VRBrighterColors` in the
installed INI. Existing INIs without a color setting keep Dark; new INIs include
`VRBrighterColors=0`. Other settings stay unchanged, and the previous INI is backed
up as `TombRaiderVR.ini.vr-options.bak`. Index mode uses the existing
`TombRaiderVRBindings` files beside the DLL.

'''+s[end:];p.write_text(s,encoding='utf-8')
