from pathlib import Path
import subprocess,sys

collection=sys.argv[1]
root=Path('C:/dev/TombRaider'+collection+'VR')
work=Path('C:/dev/TombRaider456VR/build/third-person-back-hop')
files=['src/FirstPerson.cpp','src/FirstPerson.h','src/Gamepad.cpp','README.md']
baseline={p:subprocess.check_output(['git','-C',str(root),'-c','safe.directory='+root.as_posix(),'show','HEAD:'+p]).decode('utf-8-sig') for p in files}
def replace(p,old,new):
    assert baseline[p].count(old)==1,(p,old[:90],baseline[p].count(old))
    baseline[p]=baseline[p].replace(old,new)

replace('src/FirstPerson.cpp','bool g_jumpPressed = false;',
        'bool g_jumpPressed = false;\nbool g_thirdPersonBackHopPressed = false;')
replace('src/FirstPerson.cpp','bool jumpPressed) {','bool jumpPressed, bool walkPressed) {')
replace('src/FirstPerson.cpp','    g_jumpPressed = jumpPressed;',
        '''    g_jumpPressed = jumpPressed;
    // Record the untransformed chord even when the first-person view is off.
    // A suspended first-person camera must not enable a third-person shortcut.
    g_thirdPersonBackHopPressed=walkPressed && !shifted && !jumpPressed &&
        leftY < -.20f && std::fabs(leftY)>=std::fabs(leftX);''')
if collection=='456':
    replace('src/FirstPerson.h','bool jumpPressed = false);','bool jumpPressed = false, bool walkPressed = false);')
    replace('src/Gamepad.cpp','    FirstPersonInput(fpLeftX, fpLeftY, fpRightX, shifted, jumpPressed);',
            '    const bool walkPressed = !inventoryOwned && (mine.Gamepad.wButtons & XB_X) != 0;\n    FirstPersonInput(fpLeftX, fpLeftY, fpRightX, shifted, jumpPressed, walkPressed);')
    replace('src/FirstPerson.cpp','    g_shifted = g_jumpPressed = false;','    g_shifted = g_jumpPressed = g_thirdPersonBackHopPressed = false;')
else:
    replace('src/FirstPerson.h','bool jumpPressed);','bool jumpPressed, bool walkPressed = false);')
    replace('src/Gamepad.cpp','    FirstPersonInput(lx, ly, rx, shifted, jumpPressed);',
            '    const bool walkPressed = (mine.Gamepad.wButtons & XB_X) != 0;\n    FirstPersonInput(lx, ly, rx, shifted, jumpPressed, walkPressed);')
    replace('src/FirstPerson.cpp','    g_jumpPressed = false;','    g_jumpPressed = g_thirdPersonBackHopPressed = false;')
replace('src/FirstPerson.cpp','void __cdecl Detour_LaraAboveWater(uint8_t* item, void* nativeCollision) {',
        '#include "ThirdPersonBackHop.inl"\n\nvoid __cdecl Detour_LaraAboveWater(uint8_t* item, void* nativeCollision) {')
replace('src/FirstPerson.cpp','        RoomscaleCollisionScope roomscaleCollision(item);\n        g_hLaraAboveWater.Original<Fn_LaraAboveWater>()(item, nativeCollision);',
        '        RoomscaleCollisionScope roomscaleCollision(item);\n        ThirdPersonBackHopScope backHop(item);\n        g_hLaraAboveWater.Original<Fn_LaraAboveWater>()(item, nativeCollision);')

scope='''// RG + LS down is a third-person-only, Lara-relative native back-hop.
// Do not use the HMD heading or require a first-person camera anchor.
struct ThirdPersonBackHopScope {
    uint64_t* input=nullptr;
    int16_t* axes=nullptr;
    uint64_t savedDirections=0;
    int16_t savedAxes[4]{};
    explicit ThirdPersonBackHopScope(uint8_t* item) {
        using namespace locomotion;
        if(!g_thirdPersonBackHopPressed || g_active || FIRST_PERSON_SELECTED ||
           !item || !g_boundDll || !g_boundBase || !Cfg().enabled || !Cfg().gamepadEnabled ||
           !VR().active() || !VR().poseValid() || g_shifted || g_jumpPressed ||
           InInventory() || InTitle() || IN_MOVIE || !GroundJumpControlsAvailable() ||
           item!=*Ptr<uint8_t*>(g_boundDll->laraItem) || !GroundBodyAvailable(item) ||
           !CanModifyGroundMotion(item)) return;
        auto* nativeInput=Ptr<uint64_t>(g_boundDll->input);
        constexpr uint64_t duck=DUCK_BIT;
        if(*nativeInput&(0x10u|0x40u|0x100u|0x1000u|0x4000u|duck)) return;
        const int required=*reinterpret_cast<const int16_t*>(item+22);
        // A stale required DUCK after standing in VCI must not block the chord.
        // Active posture/interaction goals and input were rejected above.
        if(required && !(HAS_CROUCH && required==71 && !CROUCH_LATCH)) return;
        const int state=*reinterpret_cast<const int16_t*>(item+off::item_anim_state);
        const int clip=*reinterpret_cast<const int16_t*>(item+off::item_anim_number);
        const bool turning=(state==6 && clip==12) || (state==7 && clip==13) ||
            (state==20 && (clip==44 || clip==69));
        const bool hopping=state==5 && (clip==88 || clip==89 || clip==90);
        if(!hopping && !OrdinaryGroundAnimation(item) && !turning) return;
        if(!hopping && !EnterGroundIdle(item)) return;
        input=nativeInput;savedDirections=*input&(Directions|Walk);
        *input=(*input&~(Directions|Walk))|Back;
        // Native modern controls otherwise rotate toward the backward stick
        // before interpreting Back. Keep camera input scoped to this tick.
        const int16_t yaw=reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos)->y_rot;
        axes=Ptr<int16_t>(g_boundDll->analogInput);
        for(int i=0;i<4;++i) savedAxes[i]=axes[i];
        axes[0]=axes[1]=0;axes[2]=axes[3]=yaw;
        *Ptr<int16_t>(g_boundDll->lara+TURN_RATE)=0;
        *Ptr<int16_t>(g_boundDll->lara+MOVE_ANGLE)=int16_t(uint16_t(yaw)+0x8000u);
        // Native STOP performs rear-floor clearance and selects FASTBACK;
        // animation, displacement, gravity and wall collision remain native.
    }
    ~ThirdPersonBackHopScope() {
        if(!input) return;
        constexpr auto mask=locomotion::Directions|locomotion::Walk;
        *input=(*input&~mask)|savedDirections;
        for(int i=0;i<4;++i) axes[i]=savedAxes[i];
    }
};
'''
values={'FIRST_PERSON_SELECTED':'(Cfg().firstPerson && g_runtimeEnabled)' if collection=='456' else 'g_runtimeEnabled',
        'IN_MOVIE':'InFMV()' if collection=='456' else 'InCutscene()',
        'DUCK_BIT':'firstperson::PhysicalCrouchHoldScope::Duck' if collection=='456' else 'firstperson::PhysicalCrouchInputScope::Duck',
        'HAS_CROUCH':'(g_boundDll->game==0 || g_boundDll->game==1)' if collection=='456' else '(BoundGame()==3)',
        'CROUCH_LATCH':'(*Ptr<uint32_t>(g_boundDll->lara+68)&0x40u)' if collection=='456' else '(*Ptr<uint32_t>(g_boundDll->lara+0x40)&firstperson::PhysicalCrouchInputScope::Latch)',
        'TURN_RATE':'off::lara_turn_rate' if collection=='456' else '252',
        'MOVE_ANGLE':'off::lara_move_angle' if collection=='456' else '254'}
for a,b in values.items():scope=scope.replace(a,b)
if collection=='123':scope=scope.replace('uint64_t','uint32_t')
baseline['src/ThirdPersonBackHop.inl']=scope
if collection=='456':
    replace('README.md','  is merged. In menus and FMVs the grips retain their individual bindings.',
        '''  is merged. In menus and FMVs the grips retain their individual bindings.
  In TR4/5 **third person only**, right grip + LS down requests Lara's native
  backward hop relative to her facing. First-person movement and vent crawling
  keep their existing controls; Jump and interaction inputs take priority.''')
else:
    baseline['README.md']+='''\n### Third-person backward hop

In TR1–3 third person only, right grip + LS down requests Lara's native backward
hop relative to her facing. First-person movement and vent crawling retain their
existing controls. Jump and interaction inputs take priority; native floor and
wall collision remain in control.
'''
for relative,text in baseline.items():
    p=root/relative
    if p.exists():
        backup=work/'before-scope-correction'/collection/relative
        backup.parent.mkdir(parents=True,exist_ok=True)
        if not backup.exists():backup.write_bytes(p.read_bytes())
    if '--apply' in sys.argv:p.write_text(text,encoding='utf-8',newline='\r\n')
    print(('Updated ' if '--apply' in sys.argv else 'Prepared ')+str(p))
