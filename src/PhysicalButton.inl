// Included after PhysicalMonitor.inl; shares calibrated left/right palm poses.
struct ButtonDll { uint32_t timestamp,sequenceCollision,sequenceCount,switchTrigger; };
constexpr ButtonDll kButtonDlls[]={
    {0x696B4999,0x8D500,0x63EC06,0}, {0x696B499C,0x8C3F0,0x63D8DB,0x8A5C0},
    {0x68C12FDA,0x8DF10,0x63FB46,0}, {0x68C12FE9,0x8C700,0x63D81B,0x8A8E0},
};
const ButtonDll* g_buttonDll=nullptr;
struct ButtonHand {
    MonitorHandSample sample{};
    bool latched=false;
    motiongun::Frame inverse{};
    firstperson::ButtonSurface face{};
};
ButtonHand g_buttonHands[2]{};
int g_buttonLevel=-1;
uint8_t* g_buttonItems=nullptr;
void ResetPhysicalButtons() {
    for(auto& hand:g_buttonHands) hand={};g_buttonItems=nullptr;g_buttonLevel=-1;
}
const firstperson::ButtonProfile* ButtonObject(uint8_t* target,const uint8_t*& object) {
    if(!g_buttonDll || !g_switchDll || !g_boundDll || !g_boundDll->objects) return nullptr;
    const int slot=*reinterpret_cast<int16_t*>(target+16),game=g_boundDll->game;
    const int first=game==0 ? 306 : 266,sequence=game==0 ? 341 : 329;
    if(!((slot>=first && slot<first+8) || (slot>=sequence && slot<sequence+3))) return nullptr;
    object=Ptr<uint8_t>(g_boundDll->objects)+slot*off::object_stride;
    if(*reinterpret_cast<const int32_t*>(object+off::object_geom+28)) return nullptr;
    const auto* profile=firstperson::FindButtonProfile(game,*reinterpret_cast<const char* const*>(object+off::object_geom),
        *reinterpret_cast<const int16_t*>(object));
    if(!profile || profile->sequence!=(slot>=sequence && slot<sequence+3)) return nullptr;
    const auto handler=*reinterpret_cast<const uint64_t*>(object+56);
    if(handler!=g_boundBase+(profile->sequence ? g_buttonDll->sequenceCollision : g_switchDll->collision)) return nullptr;
    const int type=*reinterpret_cast<int16_t*>(target+48);
    // The same keypad mesh has ordinary OCB 5/7 and preset-trip OCB 6
    // instances. Only OCB 6 owns the lift's route query and travel lock.
    // All three use SwitchCollision, not the inventory code-entry handler.
    if(profile->elevator) return game==1 && slot==268 && type>=5 && type<=7 ? profile : nullptr;
    // OCB 0/4 are levers, >=8 are valves. Unknown OCB 6 meshes stay native.
    if(!profile->sequence && !(type==1 || type==2 || type==3 || (game==1 && (type==5 || type==7)))) return nullptr;
    return profile;
}
bool ButtonReady(uint8_t* target,const firstperson::ButtonProfile& profile) {
    if((*reinterpret_cast<uint32_t*>(target+0x1820)&6) || (*reinterpret_cast<uint16_t*>(target+44)&0x100)) return false;
    const int state=*reinterpret_cast<int16_t*>(target+18),type=*reinterpret_cast<int16_t*>(target+48);
    if(state!=0 && state!=1) return false;
    if(profile.elevator) return state==1 && g_boundDll->disableLaraControl &&
        !*Ptr<int32_t>(g_boundDll->disableLaraControl);
    if(profile.sequence) return state==1 && *Ptr<uint8_t>(g_buttonDll->sequenceCount)<3;
    if(type==3) return state==0;
    if(g_boundDll->game==1 && (type==5 || type==7)) return state==1;
    return true;
}
bool ButtonFrame(uint8_t* target,const uint8_t* object,const firstperson::ButtonProfile& profile,
                 motiongun::Frame& inverse) {
    motiongun::Frame bind{},inverseBind{};
    if(!ReadIKBind(object,profile.joints,0,nullptr,nullptr,profile.joint,bind) || !motiongun::Inverse(bind,inverseBind)) return false;
    alignas(16) uint8_t probe[off::item_stride];std::memcpy(probe,target,sizeof(probe));
    SwitchPoseMatrices(probe,{*reinterpret_cast<int16_t*>(target+24),*reinterpret_cast<int16_t*>(target+26),0});
    auto world=motiongun::Multiply(motiongun::ReadRows(reinterpret_cast<float*>(probe+7832)+profile.joint*12),inverseBind);
    const auto& pos=*reinterpret_cast<const PHD_3DPOS*>(target+off::item_pos);
    world.origin=Add(world.origin,{float(pos.x_pos),float(pos.y_pos),float(pos.z_pos)});
    return firstperson::Finite(world) && motiongun::Inverse(world,inverse);
}
bool ElevatorPanelAtLara(int16_t index,uint8_t* lara) {
    if(!g_buttonDll->switchTrigger) return false;
    // ControlLiftTeleporter resolves the panel from Lara's current floor.
    // Require that same route before locking controls, even when a controller
    // can reach the panel across a doorway. Do not invent a destination.
    alignas(16) uint8_t probe[off::item_stride];std::memcpy(probe,lara,sizeof(probe));
    int16_t links[1024]{};
    using Query=int32_t (__cdecl*)(uint8_t*,int16_t*);
    const int count=reinterpret_cast<Query>(g_boundBase+g_buttonDll->switchTrigger)(probe,links);
    if(count<1 || count>1024) return false;
    auto* items=*Ptr<uint8_t*>(g_switchDll->items);
    const int itemCount=*Ptr<int32_t>(g_switchDll->count);
    if(!items || itemCount<1 || itemCount>32768) return false;
    // Match the native lift's reverse scan of ordinary switch slots.
    for(int i=count-1;i>=0;--i) {
        if(links[i]<0 || links[i]>=itemCount) return false;
        const auto* target=items+links[i]*off::item_stride;
        const int slot=*reinterpret_cast<const int16_t*>(target+16);
        if(slot>=266 && slot<=271 && *reinterpret_cast<const int16_t*>(target+48)==6)
            return links[i]==index;
    }
    return false;
}
bool ActivatePhysicalButton(int16_t index,uint8_t* target,uint8_t* lara,void* collision,const firstperson::ButtonProfile& profile) {
    const bool presetTrip=profile.elevator && *reinterpret_cast<int16_t*>(target+48)==6;
    if(!ButtonReady(target,profile) || (presetTrip && !ElevatorPanelAtLara(index,lara))) return false;
    {
        CogLaraScope saved(lara);
        *reinterpret_cast<int16_t*>(lara+18)=2;*reinterpret_cast<int16_t*>(lara+24)=103;
        *Ptr<uint64_t>(g_boundDll->input)|=0x40;g_switchCommit=target;
        if(profile.sequence)
            reinterpret_cast<Fn_SwitchCollision>(g_boundBase+g_buttonDll->sequenceCollision)(index,lara,collision);
        else g_hSwitchCollision.Original<Fn_SwitchCollision>()(index,lara,collision);
    }
    const bool activated=(*reinterpret_cast<uint32_t*>(target+0x1820)&6)==2;
    // Native control now advances the button, timer, sound and sequence state
    // once per tick. Lift control retains its travel lock and releases it at
    // arrival. Never fast-forward timers or manufacture puzzle triggers.
    if(activated) LogF("vr-button: touched item=%d sequence=%d; native button control continues",index,profile.sequence);
    return activated;
}
void AdvancePhysicalButton(uint8_t* lara,void* collision,uint64_t now) {
    if(!g_buttonDll || !PhysicalSwitchReady() || !lara || lara!=*Ptr<uint8_t*>(g_boundDll->laraItem) ||
       g_switchGripClaimed || g_cogClaimed || g_containerClaimed || g_pulleyClaimed || (*reinterpret_cast<uint32_t*>(lara+0x1820)&8)) {
        ResetPhysicalButtons();return;
    }
    const int state=*reinterpret_cast<int16_t*>(lara+18);
    if(state!=2 && state!=71 && state!=80) { ResetPhysicalButtons();return; }
    auto* items=*Ptr<uint8_t*>(g_switchDll->items);const int count=*Ptr<int32_t>(g_switchDll->count);
    if(!items || count<1 || count>32768) { ResetPhysicalButtons();return; }
    const int level=AppState(drva::app_off::level);
    if(level!=g_buttonLevel || items!=g_buttonItems) ResetPhysicalButtons();
    g_buttonItems=items;g_buttonLevel=level;
    const auto& body=*reinterpret_cast<const PHD_3DPOS*>(lara+off::item_pos);
    const float scale=LiveWorldUnitsPerMetre();
    for(int hand=0;hand<2;++hand) {
        auto& h=g_buttonHands[hand];motiongun::Vec point{};
        if(!MonitorHand(hand,point)) { h.sample={};continue; }
        const auto previous=h.sample;h.sample={point,now,true};
        if(h.latched) {
            // Deliberately withdraw from the face to press again. Jitter at
            // the touch threshold cannot keep toggling a resettable button.
            const auto local=motiongun::Transform(h.inverse,point);
            if(local.z<h.face.z-scale*.10f) h.latched=false;
            continue;
        }
        if(!previous.valid || now<=previous.time || now-previous.time>250 ||
           firstperson::Length(Sub(point,previous.world))>scale*.4f) continue;
        for(int i=0;i<count;++i) {
            auto* target=items+i*off::item_stride;const uint8_t* object=nullptr;
            const auto* profile=ButtonObject(target,object);
            if(!profile || !ButtonReady(target,*profile)) continue;
            const auto& pos=*reinterpret_cast<const PHD_3DPOS*>(target+off::item_pos);
            if(std::abs(float(body.y_pos)-pos.y_pos)>768 ||
               std::hypot(float(body.x_pos)-pos.x_pos,float(body.z_pos)-pos.z_pos)>1800) continue;
            motiongun::Frame inverse{};
            if(!ButtonFrame(target,object,*profile,inverse) ||
               !firstperson::ButtonTouch(profile->face,motiongun::Transform(inverse,previous.world),motiongun::Transform(inverse,point),scale) ||
               !CogLineOfSight(lara,point)) continue;
            h.latched=true;h.inverse=inverse;h.face=profile->face;
            ActivatePhysicalButton(int16_t(i),target,lara,collision,*profile);
            g_buttonHands[1-hand].sample={};return;
        }
    }
}
void InstallPhysicalButtons(const GameDllLayout& d,uint64_t base) {
    ResetPhysicalButtons();g_buttonDll=nullptr;
    if(!g_switchDll) return;
    for(const auto& row:kButtonDlls) if(row.timestamp==d.timestamp) g_buttonDll=&row;
    if(!g_buttonDll) return;
    const uint8_t bytes[]={0x48,0x89,0x5C,0x24,uint8_t(d.game==0 ? 0x10 : 0x08)};
    if(std::memcmp(reinterpret_cast<void*>(base+g_buttonDll->sequenceCollision),bytes,sizeof(bytes))) {
        g_buttonDll=nullptr;Log("vr-button: native function check failed; touch disabled");return;
    }
    const uint8_t triggerBytes[]={0x48,0x89,0x5C,0x24,0x10};
    if(g_buttonDll->switchTrigger && std::memcmp(reinterpret_cast<void*>(base+g_buttonDll->switchTrigger),triggerBytes,sizeof(triggerBytes))) {
        g_buttonDll=nullptr;Log("vr-button: lift trigger query check failed; touch disabled");return;
    }
    Log("vr-button: touch activation enabled for verified wall/elevator and sequence buttons");
}
