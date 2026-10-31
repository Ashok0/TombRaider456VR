from pathlib import Path
root=Path(__file__).resolve().parents[1]
files=['VRMenu.h','VRMenu.cpp','VRMenuModel.h','VRMenuSettings.h','VRMenuSettings.inl']
for repo in (root/'build/vr-options123',):
    if repo!=root:
        for name in files: (repo/'src'/name).write_bytes((root/'src'/name).read_bytes())
    p=repo/'src/Config.cpp';s=p.read_text();s=s.replace('#include "Config.h"','#include "Config.h"\n#include "VRMenuSettings.h"',1)
    at=s.rindex('} // namespace tr');s=s[:at]+'#include "VRMenuSettings.inl"\n\n'+s[at:];p.write_text(s)
    p=repo/'src/Gamepad.cpp';s=p.read_text().replace('#include "Gamepad.h"','#include "Gamepad.h"\n#include "VRMenu.h"',1)
    marker='    ammoinput::Apply(mine.Gamepad);' if repo==root else '    ApplyArmedMenu(mine.Gamepad);'
    assert marker in s
    s=s.replace(marker,'''    if(VRMenuInput(mine.Gamepad.wButtons,mine.Gamepad.sThumbLX,mine.Gamepad.sThumbLY)) {
        mine.Gamepad={};*state=mine;return ERROR_SUCCESS;
    }

'''+marker,1);p.write_text(s)
    p=repo/'src/Hooks.cpp';s=p.read_text();s='#include "VRMenu.h"\n'+s
    # Present is the existing safe frame boundary for UI and runtime maintenance.
    start=s.index('void __cdecl Detour_ogl_present()')
    at=s.index('    GameDllUpdate();',start)+len('    GameDllUpdate();')
    s=s[:at]+'\n    VRMenuUpdate();'+s[at:];p.write_text(s)
    p=repo/'src/VRSystem.cpp';s=p.read_text();s='#include "VRMenu.h"\n'+s
    s=s.replace('void VRSystem::Shutdown() {','void VRSystem::Shutdown() {\n    VRMenuShutdown();',1);p.write_text(s)
    p=repo/'TombRaiderVR.vcxproj';s=p.read_text()
    s=s.replace('<ClCompile Include="src\\Config.cpp" />','<ClCompile Include="src\\Config.cpp" />\n    <ClCompile Include="src\\VRMenu.cpp" />',1)
    s=s.replace('<ClInclude Include="src\\Config.h" />','<ClInclude Include="src\\Config.h" />\n'+''.join('    <ClInclude Include="src\\'+f+'" />\n' for f in files if f!='VRMenu.cpp'),1)
    s=s.replace('<AdditionalDependencies>opengl32.lib;','<AdditionalDependencies>gdi32.lib;opengl32.lib;',1);p.write_text(s)
    p=repo/'README.md';s=p.read_text(encoding='utf-8')
    section='''
## VR OPTIONS — controllers

At a game's title screen, **VR OPTIONS** appears in the headset beneath the
main menu. Click the **right stick** to open it. This is a VR title-screen
panel shared across the games; it does not replace their existing Options menu.
Use the **left stick** up/down to select a setting and left/right to adjust it.
**A** selects; **B** cancels without saving. Continue down through the pages to
**SAVE & CLOSE** or **CANCEL**. Inputs are consumed while the panel is open.

The panel contains Touch/Index mode, Index grip press/release thresholds,
D-pad shift and deadzone, first-person turn speed/deadzone, and controller-local
hand position and angle calibration. First-person settings apply to TR1–5.
Index mode requires the existing TombRaiderVRBindings folder beside the DLL.

Saved changes apply after restarting the game. Only changed controller keys
are written to the installed INI; other settings are preserved. The previous
INI is backed up as `TombRaiderVR.ini.vr-options.bak`. No existing setting is
changed by installing the menu. The panel is hidden outside title screens.

'''
    at=s.find('\n## ',s.find('\n')+1);assert at>=0
    s=s[:at]+ '\n'+section+s[at:];p.write_text(s,encoding='utf-8')
p=root/'README.md';s=p.read_text(encoding='utf-8');at=s.find('\n## ',s.find('\n')+1)
p.write_text(s[:at]+'\n'+section+s[at:],encoding='utf-8')
print('Wired VR title-screen controller options into both builds.')
