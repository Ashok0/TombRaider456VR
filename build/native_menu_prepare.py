from pathlib import Path
import json,hashlib
root=Path('C:/dev/TombRaider456VR')
p=root/'src/VRMenu.cpp';s=p.read_text()
s=s.replace('#include "VRMenu.h"','#include "VRMenu.h"\n#include "VRNativeMenu.h"')
s=s.replace('width=900;height=model.open ? 720 : 150;','width=900;height=720;')
a=s.index('    if(!model.open) {');b=s.index('        text(L"VR OPTIONS",{42',a)
s=s[:a]+'    {\n'+s[b:]
a=s.index('    if(!g_menu.open) {',s.index('bool AnchorOverlay()'));b=s.index('    const auto origin=',a)
s=s[:a]+s[b:]
a=s.index('bool VRMenuInput(')
s=s[:a]+'''bool VRMenuOpen() {
    std::lock_guard<std::mutex> lock(g_menuMutex);
    if(!Available() || !g_ready || g_menu.open) return false;
    g_menu.Open();g_menu.values=g_menu.original=vrmenu::ReadSettings();
    g_message.clear();g_dirty=g_reanchor=true;
    return true;
}
bool VRMenuIsOpen() {
    std::lock_guard<std::mutex> lock(g_menuMutex);
    return g_menu.open;
}
bool VRMenuBlocksInput() {
    std::lock_guard<std::mutex> lock(g_menuMutex);
    return g_menu.open || g_menu.drain;
}
'''+s[a:]
s=s.replace('    if(event==vrmenu::Event::Open) {\n        g_menu.values=g_menu.original=vrmenu::ReadSettings();g_message.clear();g_reanchor=true;\n    } else if(event==vrmenu::Event::Save) {','    if(event==vrmenu::Event::Save) {')
s=s.replace('void VRMenuUpdate() {\n    std::lock_guard','void VRMenuUpdate() {\n    VRNativeMenuUpdate();\n    std::lock_guard')
s=s.replace('    if(!InitOverlay()) return;','''    g_ready=InitOverlay();
    if(!g_ready) return;
    if(!g_menu.open) {
        if(g_shown) g_overlay->HideOverlay(g_handle);
        g_shown=false;return;
    }''')
s=s.replace('void VRMenuShutdown() {\n    std::lock_guard','void VRMenuShutdown() {\n    VRNativeMenuShutdown();\n    std::lock_guard')
s=s.replace('could not create title entry','could not create settings panel')
p.write_text(s)
# Refresh the isolated TR1-3 staging tree from its now-installed source.
import shutil
source=Path('C:/dev/TombRaider123VR');stage=root/'build/vr-options123'
baseline={}
for name in ['README.md','TombRaiderVR.vcxproj','src/VRMenu.cpp','src/VRMenu.h','src/VRMenuModel.h','src/InlineHook.cpp','src/InlineHook.h','tools/vr_menu_tests.cpp','build/test_vr_options.cmd']:
    baseline[name]=hashlib.sha256((source/name).read_bytes()).hexdigest()
    (stage/name).parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(source/name,stage/name)
(stage/'native-baseline.json').write_text(json.dumps(baseline,indent=2))
