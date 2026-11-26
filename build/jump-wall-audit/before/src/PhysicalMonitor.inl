// TR5's native MONITOR_SCREEN interaction (RICH1/RICH2), activated by touching
// its glass. The visible HD television can be room geometry: its native item
// deliberately has a DUMMY HD mesh, but retains the same classic frame/pivot.
struct MonitorDll { uint32_t timestamp,collision,trigger; };
constexpr MonitorDll kMonitorDlls[]={
    {0x696B499C,0x75230,0x8A6E0}, {0x68C12FE9,0x76910,0x8A9F0},
};
const MonitorDll* g_monitorDll=nullptr;
struct MonitorHandSample { motiongun::Vec world{};uint64_t time=0;bool valid=false; };
MonitorHandSample g_monitorHands[2]{};
int g_monitorLevel=-1;
uint8_t* g_monitorItems=nullptr;
void ResetPhysicalMonitors() {
    for(auto& h:g_monitorHands) h={};g_monitorLevel=-1;g_monitorItems=nullptr;
}
bool MonitorHand(int hand,motiongun::Vec& world) {
    motiongun::Frame wrist{};GunBasis controller{};motiongun::Vec local{};
    if(!TrackedControllerWrist(hand,wrist,controller,&local)) return false;
    auto* lara=*Ptr<uint8_t*>(g_boundDll->laraItem);
    const int slot=lara ? *reinterpret_cast<int16_t*>(lara+off::item_object) : -1;
    if(slot>=0 && g_boundDll->objects) {
        const auto* object=Ptr<uint8_t>(g_boundDll->objects)+slot*off::object_stride;
        motiongun::Frame bind{},inverse{};
        if(*reinterpret_cast<const int16_t*>(object)==15 &&
           ReadIKBind(object,15,0,nullptr,nullptr,hand ? 10 : 13,bind) && motiongun::Inverse(bind,inverse))
            local=motiongun::Transform(inverse,{hand ? 62.f : -62.f,40,0});
    }
    world=motiongun::Transform(wrist,local);
    return std::isfinite(world.x) && std::isfinite(world.y) && std::isfinite(world.z);
}
bool MonitorFrame(uint8_t* item,motiongun::Frame& world,motiongun::Frame& inverse) {
    if(!g_monitorDll || !g_switchDll || !g_boundDll || g_boundDll->game!=1 || !g_boundDll->objects ||
       !g_boundDll->anims || *reinterpret_cast<int16_t*>(item+16)!=151 ||
       (*reinterpret_cast<uint32_t*>(item+0x1820)&6) || (*reinterpret_cast<uint16_t*>(item+44)&0x100)) return false;
    const auto* object=Ptr<uint8_t>(g_boundDll->objects)+151*off::object_stride;
    const char* name=*reinterpret_cast<const char* const*>(object+off::object_geom);
    if(*reinterpret_cast<const uint64_t*>(object+56)!=g_boundBase+g_monitorDll->collision ||
       *reinterpret_cast<const int16_t*>(object)!=1 || *reinterpret_cast<const int32_t*>(object+off::object_geom+28) ||
       !name || (std::strcmp(name,"DUMMY") && std::strcmp(name,"MONITOR_SCREEN"))) return false;
    const int base=*reinterpret_cast<const int16_t*>(object+66);
    const auto* animations=*Ptr<firstperson::SwitchAnim*>(g_boundDll->anims);
    if(base<0 || !animations || *reinterpret_cast<int16_t*>(item+24)!=base) return false;
    const auto& idle=animations[base];
    const int16_t bounds[]={-202,205,-924,-223,358,516};
    if(!idle.frames || idle.first!=idle.last || *reinterpret_cast<int16_t*>(item+26)!=idle.first ||
       (idle.interpolation>>8)<10 || std::memcmp(idle.frames,bounds,sizeof(bounds))) return false;
    motiongun::Frame bind{},inverseBind{};
    if(!ReadIKBind(object,1,0,nullptr,nullptr,0,bind) || !motiongun::Inverse(bind,inverseBind)) return false;
    alignas(16) uint8_t probe[off::item_stride];std::memcpy(probe,item,sizeof(probe));
    SwitchPoseMatrices(probe,{int16_t(base),idle.first,0});
    world=motiongun::Multiply(motiongun::ReadRows(reinterpret_cast<float*>(probe+7832)),inverseBind);
    const auto& pos=*reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos);
    world.origin=Add(world.origin,{float(pos.x_pos),float(pos.y_pos),float(pos.z_pos)});
    return firstperson::Finite(world) && motiongun::Inverse(world,inverse);
}
bool ActivatePhysicalMonitor(int16_t index,uint8_t* target,uint8_t* lara,void* collision) {
    // Native collision supplies the one-shot item flags/status. Restore Lara
    // BEFORE firing the level trigger: a real cutscene/teleport must survive.
    {
        CogLaraScope saved(lara);
        *reinterpret_cast<int16_t*>(lara+18)=2;*reinterpret_cast<int16_t*>(lara+24)=103;
        *Ptr<uint64_t>(g_boundDll->input)|=0x40;g_switchCommit=target;
        reinterpret_cast<Fn_SwitchCollision>(g_boundBase+g_monitorDll->collision)(index,lara,collision);
    }
    if((*reinterpret_cast<uint32_t*>(target+0x1820)&6)!=2 || !(*reinterpret_cast<uint16_t*>(target+44)&0x20)) return false;
    // This is the exact native effect at Lara animation 197, first+24.
    // There is no need to play her scripted button-press/auto-alignment first.
    const auto pos=*reinterpret_cast<const PHD_3DPOS*>(target+off::item_pos);
    const int16_t room=*reinterpret_cast<int16_t*>(target+off::item_room);
    using Trigger=void (__cdecl*)(int32_t,int32_t,int32_t,int16_t,int16_t,int16_t);
    LogF("vr-monitor: screen touched item=%d; native monitor trigger",index);
    reinterpret_cast<Trigger>(g_boundBase+g_monitorDll->trigger)(pos.x_pos,pos.y_pos,pos.z_pos,room,1,0);
    return true;
}
void AdvancePhysicalMonitor(uint8_t* lara,void* collision,uint64_t now) {
    if(!g_monitorDll || !PhysicalSwitchReady() || !lara || lara!=*Ptr<uint8_t*>(g_boundDll->laraItem) ||
       g_switchGripClaimed || g_cogClaimed || g_containerClaimed || g_pulleyClaimed ||
       (*reinterpret_cast<uint32_t*>(lara+0x1820)&8)) { ResetPhysicalMonitors();return; }
    const int state=*reinterpret_cast<int16_t*>(lara+18);
    if(state!=2 && state!=71 && state!=80) { ResetPhysicalMonitors();return; }
    auto* items=*Ptr<uint8_t*>(g_switchDll->items);const int count=*Ptr<int32_t>(g_switchDll->count);
    if(!items || count<1 || count>32768) { ResetPhysicalMonitors();return; }
    const int level=AppState(drva::app_off::level);
    if(level!=g_monitorLevel || items!=g_monitorItems) ResetPhysicalMonitors();
    g_monitorLevel=level;g_monitorItems=items;
    const auto& body=*reinterpret_cast<const PHD_3DPOS*>(lara+off::item_pos);
    const float scale=LiveWorldUnitsPerMetre();
    for(int hand=0;hand<2;++hand) {
        motiongun::Vec point{};auto& sample=g_monitorHands[hand];
        if(!MonitorHand(hand,point)) { sample={};continue; }
        const auto previous=sample;sample={point,now,true};
        if(!previous.valid || now<=previous.time || now-previous.time>250 ||
           firstperson::Length(Sub(point,previous.world))>scale*.4f) continue;
        for(int i=0;i<count;++i) {
            auto* target=items+i*off::item_stride;
            if(*reinterpret_cast<int16_t*>(target+16)!=151) continue;
            const auto& pos=*reinterpret_cast<const PHD_3DPOS*>(target+off::item_pos);
            if(std::abs(float(body.y_pos)-pos.y_pos)>768 ||
               std::hypot(float(body.x_pos)-pos.x_pos,float(body.z_pos)-pos.z_pos)>1800) continue;
            motiongun::Frame frame{},inverse{};
            if(!MonitorFrame(target,frame,inverse) ||
               !firstperson::MonitorScreenTouch(motiongun::Transform(inverse,previous.world),motiongun::Transform(inverse,point),scale) ||
               !CogLineOfSight(lara,point)) continue;
            ActivatePhysicalMonitor(int16_t(i),target,lara,collision);
            // A trigger can change the level/camera. Do not access old items or
            // process the other hand again in this simulation tick.
            ResetPhysicalMonitors();return;
        }
    }
}
void InstallPhysicalMonitors(const GameDllLayout& d,uint64_t base) {
    g_monitorDll=nullptr;ResetPhysicalMonitors();
    if(!g_switchDll || d.game!=1) return;
    for(const auto& row:kMonitorDlls) if(row.timestamp==d.timestamp) g_monitorDll=&row;
    if(!g_monitorDll) return;
    const uint8_t collision[]={0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10};
    const uint8_t trigger[]={0x40,0x53,0x56,0x57,0x48,0x83,0xEC,0x30};
    if(std::memcmp(reinterpret_cast<void*>(base+g_monitorDll->collision),collision,sizeof(collision)) ||
       std::memcmp(reinterpret_cast<void*>(base+g_monitorDll->trigger),trigger,sizeof(trigger))) {
        g_monitorDll=nullptr;Log("vr-monitor: native function checks failed; touch disabled");return;
    }
    Log("vr-monitor: either controller can touch the native monitor screen");
}
