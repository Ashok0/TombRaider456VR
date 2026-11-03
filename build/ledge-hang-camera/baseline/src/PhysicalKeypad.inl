// TR5 numeric puzzle keypads. Drive the native keypad input routine in a
// scoped, private session: it keeps digit rules, clear/enter, sounds and the
// one-second result sequence without entering inventory or moving Lara.
struct KeypadDll {
    uint32_t timestamp,collision,step,stepSize;uint64_t stepHash;
    uint32_t finish,finishSize;uint64_t finishHash;
    uint32_t mask;uint64_t maskHash;
    uint32_t draw,drawSize;uint64_t drawHash;
    uint32_t trigger,triggerIndex,level,birthdayDisplay,birthdayTimer,richCut,cutTrigger;
    uint32_t count,y,pause,x,combination,digits,mesh;
    uint32_t left,right,up,down,select,menu,use;
};
constexpr KeypadDll kKeypadDlls[]={
#include "KeypadAddresses.inl"
};
const KeypadDll* g_keypadDll=nullptr;
hook::InlineHook g_hKeypadDraw;
uint8_t* g_keypadDrawItem=nullptr;
struct KeypadHand {
    MonitorHandSample sample{};
    bool latched=false;
    motiongun::Frame inverse{};
};
struct PhysicalKeypad {
    uint8_t* items=nullptr;uint8_t* lara=nullptr;
    int level=-1,index=-1,combination=0;
    int16_t animation=0,frame=0,room=0;
    PHD_3DPOS position{};
    keypad::Entry entry{};
    keypad::PressLight light{};
    bool submitted=false;
} g_keypad;
KeypadHand g_keypadHands[2]{};
void ResetPhysicalKeypad() { g_keypad={};for(auto& h:g_keypadHands) h={}; }

bool KeypadObject(uint8_t* item,const uint8_t*& object) {
    if(!g_keypadDll || !g_boundDll || g_boundDll->game!=1 || !g_boundDll->objects || !item ||
       *reinterpret_cast<int16_t*>(item+16)!=249 ||
       (*reinterpret_cast<uint32_t*>(item+0x1820)&6) || (*reinterpret_cast<uint16_t*>(item+44)&0x100)) return false;
    const int level=*Ptr<uint8_t>(g_keypadDll->level),code=*reinterpret_cast<int16_t*>(item+48);
    if((level!=11 && level!=12) || code<1 || code>9999) return false;
    object=Ptr<uint8_t>(g_boundDll->objects)+249*off::object_stride;
    const char* name=*reinterpret_cast<const char* const*>(object+off::object_geom);
    return *reinterpret_cast<int16_t const*>(object)==25 &&
        !*reinterpret_cast<int32_t const*>(object+off::object_geom+28) && name && !std::strcmp(name,"PUZZLE_HOLE8_1") &&
        *reinterpret_cast<uint64_t const*>(object+56)==g_boundBase+g_keypadDll->collision;
}
uint8_t* CurrentKeypad() {
    if(!g_keypadDll || !g_switchDll || g_keypad.index<0 || !g_keypad.items ||
       AppState(drva::app_off::level)!=g_keypad.level || *Ptr<uint8_t*>(g_boundDll->laraItem)!=g_keypad.lara ||
       *Ptr<uint8_t*>(g_switchDll->items)!=g_keypad.items || g_keypad.index>=*Ptr<int32_t>(g_switchDll->count)) return nullptr;
    auto* item=g_keypad.items+g_keypad.index*off::item_stride;const uint8_t* object=nullptr;
    if(!KeypadObject(item,object) || *reinterpret_cast<int16_t*>(item+48)!=g_keypad.combination ||
       *reinterpret_cast<int16_t*>(item+24)!=g_keypad.animation || *reinterpret_cast<int16_t*>(item+26)!=g_keypad.frame ||
       *reinterpret_cast<int16_t*>(item+28)!=g_keypad.room ||
       std::memcmp(item+off::item_pos,&g_keypad.position,sizeof(g_keypad.position))) return nullptr;
    return item;
}
bool KeypadFrame(uint8_t* item,const uint8_t* object,motiongun::Frame& inverse) {
    motiongun::Frame bind{},inverseBind{};
    if(!ReadIKBind(object,25,0,nullptr,nullptr,0,bind) || !motiongun::Inverse(bind,inverseBind)) return false;
    alignas(16) uint8_t probe[off::item_stride];std::memcpy(probe,item,sizeof(probe));
    if(!SwitchPoseMatrices(probe,{*reinterpret_cast<int16_t*>(item+24),*reinterpret_cast<int16_t*>(item+26),0})) return false;
    auto world=motiongun::Multiply(motiongun::ReadRows(reinterpret_cast<float*>(probe+7832)),inverseBind);
    const auto& p=*reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos);
    world.origin=Add(world.origin,{float(p.x_pos),float(p.y_pos),float(p.z_pos)});
    return firstperson::Finite(world) && motiongun::Inverse(world,inverse);
}
bool KeypadFinger(int hand,motiongun::Vec& world) {
    motiongun::Frame wrist{},bind{},inverse{};GunBasis controller{};
    if(!TrackedControllerWrist(hand,wrist,controller)) return false;
    auto* lara=*Ptr<uint8_t*>(g_boundDll->laraItem);const int slot=*reinterpret_cast<int16_t*>(lara+16);
    if(slot<0) return false;
    const auto* object=Ptr<uint8_t>(g_boundDll->objects)+slot*off::object_stride;
    if(!ReadIKBind(object,15,0,nullptr,nullptr,hand ? 10 : 13,bind) || !motiongun::Inverse(bind,inverse)) return false;
    // Distal fingertip of the rendered REST hand. Bare/gloved variants differ
    // by <6 units here; this central point remains inside their fingertip pad.
    world=motiongun::Transform(wrist,motiongun::Transform(inverse,{hand ? 57.f : -57.f,79.f,10.f}));
    return keypad::Finite(world);
}
// Called only from gameplay, never from an inventory/render callback. Save
// every native keypad/input variable the routine reads or writes; sound is
// its only lasting side effect. In particular, its menu-exit flag cannot
// close a menu or select an inventory item outside our session.
bool StepPhysicalKeypad(keypad::Entry& entry,int key,int combination) {
    const auto& d=*g_keypadDll;
    const uint32_t bytes[]={d.count,d.y,d.pause,d.x,d.left,d.right,d.up,d.down,d.select,d.menu,d.use};
    uint8_t saved[11]{};for(int i=0;i<11;++i) saved[i]=*Ptr<uint8_t>(bytes[i]);
    const auto oldDigits=*Ptr<uint32_t>(d.digits),oldMesh=*Ptr<uint32_t>(d.mesh);
    const int oldCombination=*Ptr<int32_t>(d.combination);
    *Ptr<uint8_t>(d.count)=entry.count;*Ptr<uint8_t>(d.pause)=entry.pause;
    *Ptr<uint8_t>(d.x)=key>=0 ? uint8_t(key%3) : entry.x;
    *Ptr<uint8_t>(d.y)=key>=0 ? uint8_t(key/3) : entry.y;
    std::memcpy(Ptr<uint8_t>(d.digits),entry.digits,4);
    *Ptr<int32_t>(d.combination)=combination;
    for(int i=4;i<11;++i) *Ptr<uint8_t>(bytes[i])=0;
    *Ptr<uint8_t>(d.select)=key>=0;*Ptr<uint8_t>(d.menu)=1;
    reinterpret_cast<void (__cdecl*)()>(g_boundBase+d.step)();
    entry.count=*Ptr<uint8_t>(d.count);entry.pause=*Ptr<uint8_t>(d.pause);
    entry.x=*Ptr<uint8_t>(d.x);entry.y=*Ptr<uint8_t>(d.y);
    std::memcpy(entry.digits,Ptr<uint8_t>(d.digits),4);entry.mesh=*Ptr<uint32_t>(d.mesh);
    const bool finished=*Ptr<uint8_t>(d.use)!=0 && *Ptr<uint8_t>(d.menu)==0;
    for(int i=0;i<11;++i) *Ptr<uint8_t>(bytes[i])=saved[i];
    *Ptr<uint32_t>(d.digits)=oldDigits;*Ptr<uint32_t>(d.mesh)=oldMesh;*Ptr<int32_t>(d.combination)=oldCombination;
    return finished;
}
uint16_t* KeypadFloorTrigger(uint8_t* target) {
    const auto p=*reinterpret_cast<const PHD_3DPOS*>(target+off::item_pos);
    int16_t room=*reinterpret_cast<int16_t*>(target+28);
    const auto saved=*Ptr<uint16_t*>(g_keypadDll->triggerIndex);
    void* floor=reinterpret_cast<Fn_GetFloor>(g_boundBase+g_boundDll->getFloor)(p.x_pos,p.y_pos,p.z_pos,&room);
    uint16_t* trigger=nullptr;
    if(floor) {
        reinterpret_cast<int32_t (__cdecl*)(void*,int32_t,int32_t,int32_t)>(g_boundBase+g_boundDll->getHeight)(floor,p.x_pos,p.y_pos,p.z_pos);
        trigger=*Ptr<uint16_t*>(g_keypadDll->triggerIndex);
    }
    *Ptr<uint16_t*>(g_keypadDll->triggerIndex)=saved;
    // FinishInventory invokes this as a HEAVY trigger. Do not activate an
    // arbitrary neighbouring pad, switch, keyhole or camera trigger.
    return trigger && (*trigger&0x1f)==4 && ((*trigger>>8)&0x3f)==5 ? trigger : nullptr;
}
void FinishPhysicalKeypad(uint8_t* target,const keypad::Entry& entry,int combination) {
    const auto& d=*g_keypadDll;
    // Preserve the native optional birthday code as well as ordinary codes.
    // These are the keypad-only effects of FinishInventory; the rest of that
    // function requires an allocated inventory ring and must never run here.
    if(*Ptr<uint8_t>(d.level)==12 && entry.Birthday()) {
        auto* app=*Ptr<uint8_t*>(g_motionDll->app);
        if(app) {
            const auto play=*reinterpret_cast<void (__cdecl**)(int32_t,int32_t)>(app+0x4b0);
            if(play) play(110,100);
        }
        if(!*Ptr<int16_t>(d.birthdayDisplay)) { *Ptr<int16_t>(d.birthdayTimer)=0;*Ptr<int16_t>(d.birthdayDisplay)=1; }
    }
    if(entry.Code()!=combination) { Log("vr-keypad: incorrect/incomplete code; ready to retry");return; }
    auto* trigger=KeypadFloorTrigger(target);
    if(!trigger) { Log("vr-keypad: no verified keypad floor trigger; native Action remains available");return; }
    Log("vr-keypad: correct code; dispatching native keypad trigger");
    *Ptr<uint8_t>(d.cutTrigger)=0;
    reinterpret_cast<void (__cdecl*)(uint16_t*,int32_t,int32_t)>(g_boundBase+d.trigger)(trigger,1,0);
    if(!*Ptr<uint8_t>(d.cutTrigger) && *Ptr<uint8_t>(d.richCut)) *Ptr<uint8_t>(d.richCut)=0;
}
bool KeypadNear(uint8_t* item,uint8_t* lara) {
    const auto& a=*reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos);
    const auto& b=*reinterpret_cast<const PHD_3DPOS*>(lara+off::item_pos);
    return std::abs(float(a.y_pos)-b.y_pos)<1024 && std::hypot(float(a.x_pos)-b.x_pos,float(a.z_pos)-b.z_pos)<1800;
}
bool AdvancePhysicalKeypad(uint8_t* lara,uint64_t now) {
    if(!g_keypadDll || (*Ptr<uint8_t>(g_keypadDll->level)!=11 && *Ptr<uint8_t>(g_keypadDll->level)!=12) ||
       !PhysicalSwitchReady() || !lara || !g_boundDll->getHeight ||
       lara!=*Ptr<uint8_t*>(g_boundDll->laraItem) || !UnarmedIKReady() || CurrentMotionWeapon()==7 ||
       g_switchGripClaimed || g_cogClaimed || g_containerClaimed || g_pulleyClaimed || PhysicalCrowbarOwns(lara) ||
       (*reinterpret_cast<uint32_t*>(lara+0x1820)&8)) { ResetPhysicalKeypad();return false; }
    const int state=*reinterpret_cast<int16_t*>(lara+18);
    if(state!=2 && state!=71 && state!=80) { ResetPhysicalKeypad();return false; }
    auto* items=*Ptr<uint8_t*>(g_switchDll->items);const int count=*Ptr<int32_t>(g_switchDll->count);
    if(!items || count<1 || count>32768) { ResetPhysicalKeypad();return false; }
    if(g_keypad.index>=0 && (!CurrentKeypad() || !KeypadNear(CurrentKeypad(),lara))) ResetPhysicalKeypad();
    if(auto* target=CurrentKeypad()) {
        if(g_keypad.submitted) return false;
        if(StepPhysicalKeypad(g_keypad.entry,-1,g_keypad.combination)) {
            const auto entry=g_keypad.entry;const int code=g_keypad.combination;
            g_keypad.entry={};g_keypad.submitted=entry.Code()==code && KeypadFloorTrigger(target);
            // Native triggers may start a cutscene or change level. All local
            // cleanup is finished first; never touch target/Lara afterward.
            FinishPhysicalKeypad(target,entry,code);return true;
        }
    }
    const float scale=LiveWorldUnitsPerMetre();
    for(int hand=0;hand<2;++hand) {
        auto& h=g_keypadHands[hand];motiongun::Vec point{};
        if(!KeypadFinger(hand,point)) { h={};continue; }
        const auto previous=h.sample;h.sample={point,now,true};
        if(h.latched) { if(motiongun::Transform(h.inverse,point).y<keypad::Face-scale*.025f) h.latched=false;continue; }
        if(!previous.valid || now<=previous.time || now-previous.time>250 || g_keypad.entry.pause) continue;
        for(int i=0;i<count;++i) {
            auto* target=items+i*off::item_stride;const uint8_t* object=nullptr;
            if(!KeypadNear(target,lara) || !KeypadObject(target,object)) continue;
            motiongun::Frame inverse{};
            if(!KeypadFrame(target,object,inverse)) continue;
            const int key=keypad::Touch(motiongun::Transform(inverse,previous.world),motiongun::Transform(inverse,point),scale);
            if(key<0 || !CogLineOfSight(lara,point)) continue;
            if(CurrentKeypad()!=target) {
                g_keypad={};g_keypad.items=items;g_keypad.lara=lara;g_keypad.index=i;
                g_keypad.level=AppState(drva::app_off::level);g_keypad.combination=*reinterpret_cast<int16_t*>(target+48);
                g_keypad.animation=*reinterpret_cast<int16_t*>(target+24);g_keypad.frame=*reinterpret_cast<int16_t*>(target+26);
                g_keypad.room=*reinterpret_cast<int16_t*>(target+28);g_keypad.position=*reinterpret_cast<const PHD_3DPOS*>(target+off::item_pos);
            }
            h.latched=true;h.inverse=inverse;
            StepPhysicalKeypad(g_keypad.entry,key,g_keypad.combination);
            g_keypad.light={key,now};
            VR().ButtonPressHaptic();
            LogF("vr-keypad: physical key=%d item=%d entered=%u",key,i,unsigned(g_keypad.entry.count));
            g_keypadHands[1-hand].sample={};return false;
        }
    }
    return false;
}
class KeypadMeshScope {
    uint8_t* item=nullptr;uint32_t saved=0;
    uint8_t* previousDraw=g_keypadDrawItem;
public:
    KeypadMeshScope(uint8_t* drawn,int32_t& useMeshBits) {
        g_keypadDrawItem=nullptr;
        if(!g_keypadDll || g_keypad.index<0 || !drawn || g_keypad.submitted ||
           !g_active || !Gate() || DrawingNativeShadow() || drawn!=CurrentKeypad()) return;
        item=drawn;g_keypadDrawItem=drawn;saved=*reinterpret_cast<uint32_t*>(item+12);
        *reinterpret_cast<uint32_t*>(item+12)=g_keypad.entry.mesh;useMeshBits=1;
    }
    ~KeypadMeshScope() { if(item) *reinterpret_cast<uint32_t*>(item+12)=saved;g_keypadDrawItem=previousDraw; }
};
bool KeypadLight(float out[4],uint64_t now) {
    for(int i=0;i<4;++i) out[i]=0;
    if(!g_keypadDrawItem || !g_active || !Gate() || DrawingNativeShadow() ||
       g_keypad.submitted || g_keypadDrawItem!=CurrentKeypad()) return false;
    const float strength=g_keypad.light.Strength(now);
    if(strength<=0) return false;
    // Both the raised and depressed native key mesh use the same light.
    out[0]=float(g_keypad.light.key+1);out[1]=float(g_keypad.light.key+13);out[2]=strength;
    return true;
}
void __cdecl Detour_KeypadDraw(uint8_t* item,int32_t renderPass) {
    int32_t use=1;const KeypadMeshScope scope(item,use);
    g_hKeypadDraw.Original<void (__cdecl*)(uint8_t*,int32_t)>()(item,renderPass);
}
void InstallPhysicalKeypad(const GameDllLayout& d,uint64_t base) {
    ResetPhysicalKeypad();g_keypadDll=nullptr;
    if(d.game!=1 || !g_switchDll || !g_hDrawCreatureHD.installed()) return;
    for(const auto& k:kKeypadDlls) if(k.timestamp==d.timestamp &&
       flarethrow::Fingerprint(reinterpret_cast<uint8_t*>(base+k.step),k.stepSize)==k.stepHash &&
       flarethrow::Fingerprint(reinterpret_cast<uint8_t*>(base+k.finish),k.finishSize)==k.finishHash &&
       flarethrow::Fingerprint(reinterpret_cast<uint8_t*>(base+k.mask),97)==k.maskHash &&
       flarethrow::Fingerprint(reinterpret_cast<uint8_t*>(base+k.draw),k.drawSize)==k.drawHash) {
        // Scope the whole static-item draw, including material passes. MaskJoints
        // itself returns before drawing and its callers keep a live count in r10.
        constexpr uint8_t pdb[]={0x48,0x89,0x5c,0x24,0x18}; // mov [rsp+24],rbx
        constexpr uint8_t retail[]={0x40,0x53,0x55,0x56,0x57}; // push rbx/rbp/rsi/rdi
        if(!g_hKeypadDraw.Install(reinterpret_cast<void*>(base+k.draw),reinterpret_cast<void*>(&Detour_KeypadDraw),
                                 5,k.timestamp==0x696b499c ? pdb : retail,5,"PhysicalKeypadDraw")) break;
        g_keypadDll=&k;Log("vr-keypad: physical individual keys enabled (native code validation/effects)");return;
    }
    Log("vr-keypad: native layout mismatch; Action keypad retained");
}
