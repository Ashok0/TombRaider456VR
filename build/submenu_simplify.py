from pathlib import Path
root=Path('C:/dev/TombRaider456VR')
(root/'src/VRMenuModel.h').write_text('''#pragma once
#include <cstdint>
namespace tr::vrmenu {
enum class Action { None,Change,Back };
struct ControllerPage {
    bool open=false,index=false,saveFailed=false;
    void Open(bool useIndex) { open=true;index=useIndex;saveFailed=false; }
    Action Input(uint64_t buttons) {
        if(!open) return Action::None;
        if(buttons&0x200000) { open=false;return Action::Back; }
        if(buttons&0xc0100000) return Action::Change;
        return Action::None;
    }
    const char* Value() const { return index ? "Valve Index" : "Meta Touch"; }
    const char* Message() const {
        return saveFailed ? "Could not save Controller Type" : "Restart the game to apply changes";
    }
};
}
''')
(root/'src/VRMenu.cpp').write_text('''#include "VRMenu.h"
#include "VRNativeMenu.h"
namespace tr {
void VRMenuUpdate() { VRNativeMenuUpdate(); }
void VRMenuShutdown() { VRNativeMenuShutdown(); }
}
''')
(root/'src/VRMenu.h').write_text('''#pragma once
namespace tr {
void VRMenuUpdate();
void VRMenuShutdown();
}
''')
(root/'src/VRNativeMenuModel.h').write_text('''#pragma once
#include <cstdint>
namespace tr::vrmenu {
// The extra Options row follows the twelve existing native settings. Their
// indices and action handlers remain unchanged.
struct OptionsSelection {
    bool vr=false;
    bool Input(int choice,uint64_t& input) {
        constexpr uint64_t up=0x10000000,down=0x20000000,confirm=0x100000;
        if(choice!=12) vr=false;
        if(vr) {
            if(input&up) { vr=false;input&=~(up|down|confirm); }
            else if(input&down) input&=~(down|confirm);
            else if(input&confirm) { input&=~confirm;return true; }
            input&=~uint64_t(0xc0000000);
        } else if(choice==12 && (input&down) && !(input&up)) {
            vr=true;input&=~(up|down|confirm|0xc0000000ull);
        }
        return false;
    }
};
}
''')
p=root/'src/VRMenuSettings.inl';s=p.read_text();a=s.index('        {L"ValveIndexControllers"');b=s.index('\n    }};',a)
s=s[:a]+'        {L"ValveIndexControllers",L"Controller Type",0,1,1,true,[]{return Cfg().valveIndexControllers ? 1.f : 0.f;}}'+s[b:]
s=s.replace('return values[2]<values[1];','return true;')
s=s.replace('    result[2]=std::min(result[2],result[1]-.05f);\n','')
s=s.replace('    const bool gripChanged=values[1]!=original[1] || values[2]!=original[2];\n','')
s=s.replace(' || ((i==1 || i==2) && gripChanged)','')
s=s.replace('VR OPTIONS: controller settings saved','VR Options: Controller Type saved');p.write_text(s)
for d in [root,root/'build/vr-options123']:
 p=d/'src/Gamepad.cpp';s=p.read_text();s=s.replace('#include "VRMenu.h"\n','')
 block='    if(VRMenuInput(mine.Gamepad.wButtons,mine.Gamepad.sThumbLX,mine.Gamepad.sThumbLY)) {\n        mine.Gamepad={};*state=mine;return ERROR_SUCCESS;\n    }\n\n'
 assert block in s
 p.write_text(s.replace(block,''))
