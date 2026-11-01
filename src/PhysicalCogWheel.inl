// Included after PhysicalSwitch.inl. Cog wheels have a repeating pull and a
// timed door, unlike the end-stop wall/floor levers.
struct CogDll { uint32_t timestamp,collision,control; };
constexpr CogDll kCogDlls[]={
    {0x696B4999,0x8D7E0,0x8D6A0}, {0x696B499C,0x8C6F0,0x8C5A0},
    {0x68C12FDA,0x8E1F0,0x8E0B0}, {0x68C12FE9,0x8CA00,0x8C8B0},
};
const CogDll* g_cogDll=nullptr;
hook::InlineHook g_hCogCollision,g_hCogControl;
struct PhysicalCog {
    SwitchCandidate identity{};
    PHD_3DPOS position{};
    int16_t object=-1,animation=0,frame=0,loop=0;
    int joint=0,count=0,phase=-1;
    bool started=false;
    float direction=1,progress=0;
    float angles[54]{};
    motiongun::Frame reference{};
};
std::vector<PhysicalCog> g_physicalCogs;
SwitchCandidate g_cogGrab{};
bool g_cogGripDown=false,g_cogClaimed=false;
uint64_t g_cogLastTime=0;
motiongun::Vec g_cogLocal{},g_cogGrip{},g_cogLastHand{},g_cogHandOffset{};
firstperson::CogDrive g_cogDrive;

bool CogObject(uint8_t* item,const uint8_t*& object,int& joint) {
    if(!item || !g_cogDll || !g_switchDll || !g_boundDll->objects) return false;
    const int slot=*reinterpret_cast<int16_t*>(item+16);
    // Native registration: TR4 COG_SWITCH=317, TR5 COG_SWITCH=278.
    if(slot!=(g_boundDll->game==0 ? 317 : 278)) return false;
    object=Ptr<uint8_t>(g_boundDll->objects)+slot*off::object_stride;
    if(*reinterpret_cast<const uint64_t*>(object+56)!=g_boundBase+g_cogDll->collision ||
       *reinterpret_cast<const uint64_t*>(object+24)!=g_boundBase+g_cogDll->control ||
       *reinterpret_cast<const int32_t*>(object+off::object_geom+28)>0) return false;
    const int joints=*reinterpret_cast<const int16_t*>(object);
    if(joints!=3 && joints!=4) return false;
    joint=joints==4 ? 1 : 2;
    return true;
}

uint8_t* CogItem(const PhysicalCog& cog) {
    auto* item=SwitchItem(cog.identity);
    if(!item || *reinterpret_cast<int16_t*>(item+16)!=cog.object ||
       std::memcmp(item+off::item_pos,&cog.position,sizeof(cog.position)) ||
       *reinterpret_cast<int16_t*>(item+24)!=cog.animation ||
       *reinterpret_cast<int16_t*>(item+26)!=cog.frame ||
       *reinterpret_cast<int16_t*>(item+18)!=(cog.started ? 1 : 0) ||
       *reinterpret_cast<int16_t*>(item+20)!=(cog.started ? 1 : 0) ||
       (*reinterpret_cast<uint32_t*>(item+0x1820)&6)!=(cog.started ? 2u : 0u)) return nullptr;
    return item;
}
PhysicalCog* FindCog(int index) {
    for(auto& cog:g_physicalCogs) if(cog.identity.index==index && CogItem(cog)) return &cog;
    return nullptr;
}
void ReleaseCogHand() { g_cogClaimed=false;g_cogGrab={};g_cogDrive.Release(); }

// Both native cog functions temporarily animate/lock Lara. Keep their wheel,
// active-list, sound and door side effects; restore only Lara's transient use
// pose and the synthetic Action input before any camera or locomotion runs.
class CogLaraScope {
    uint8_t* lara;
    uint8_t animation[10],angles[12];
    int16_t status;
    uint32_t flags;
    uint64_t input;
    uint8_t* commit;
public:
    explicit CogLaraScope(uint8_t* item):lara(item) {
        std::memcpy(animation,lara+18,sizeof(animation));
        std::memcpy(angles,Ptr<uint8_t>(g_boundDll->lara+224),sizeof(angles));
        status=*Ptr<int16_t>(g_boundDll->lara+2);
        flags=*Ptr<uint32_t>(g_boundDll->lara+off::lara_movement_flags);
        input=*Ptr<uint64_t>(g_boundDll->input);commit=g_switchCommit;
    }
    ~CogLaraScope() {
        std::memcpy(lara+18,animation,sizeof(animation));
        std::memcpy(Ptr<uint8_t>(g_boundDll->lara+224),angles,sizeof(angles));
        *Ptr<int16_t>(g_boundDll->lara+2)=status;
        *Ptr<uint32_t>(g_boundDll->lara+off::lara_movement_flags)=flags;
        *Ptr<uint64_t>(g_boundDll->input)=input;g_switchCommit=commit;
    }
};

bool CogFrame(uint8_t* item,int joint,motiongun::Frame& frame) {
    alignas(16) uint8_t probe[off::item_stride];std::memcpy(probe,item,sizeof(probe));
    SwitchPoseMatrices(probe,{*reinterpret_cast<int16_t*>(item+24),
        *reinterpret_cast<int16_t*>(item+26),*reinterpret_cast<int16_t*>(item+18)});
    frame=motiongun::ReadRows(reinterpret_cast<float*>(probe+7832)+joint*12);
    const auto& pos=*reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos);
    frame.origin=Add(frame.origin,{float(pos.x_pos),float(pos.y_pos),float(pos.z_pos)});
    return firstperson::Finite(frame);
}

bool BuildCog(uint8_t* item,SwitchCandidate identity,PhysicalCog& cog) {
    const uint8_t* object=nullptr;int joint=0;
    if(!CogObject(item,object,joint)) return false;
    const int base=*reinterpret_cast<const int16_t*>(object+66);
    if(base<0 || *reinterpret_cast<int16_t*>(item+24)!=base ||
       *reinterpret_cast<int16_t*>(item+18)!=0 || (*reinterpret_cast<uint32_t*>(item+0x1820)&6)) return false;
    const auto* animations=*Ptr<firstperson::SwitchAnim*>(g_boundDll->anims);
    if(!animations) return false;
    const auto& idle=animations[base];const auto& loop=animations[base+1];
    const auto& lara=animations[321];
    if(!idle.frames || !loop.frames || !lara.frames || idle.state!=0 || idle.first!=idle.last ||
       loop.state!=1 || loop.last-loop.first!=53 || loop.jump!=base+1 || loop.jumpFrame!=loop.first ||
       (loop.interpolation&255)!=1 || loop.velocity || loop.acceleration || loop.xVelocity || loop.xAcceleration ||
       lara.state!=96 || lara.last-lara.first!=53) return false;
    cog={};cog.identity=identity;cog.position=*reinterpret_cast<PHD_3DPOS*>(item+off::item_pos);
    cog.object=*reinterpret_cast<int16_t*>(item+16);cog.joint=joint;cog.loop=int16_t(base+1);
    cog.animation=int16_t(base);cog.frame=*reinterpret_cast<int16_t*>(item+26);cog.count=54;
    alignas(16) uint8_t probe[off::item_stride];std::memcpy(probe,item,sizeof(probe));
    motiongun::Frame start{};float raw[54]{};
    for(int i=0;i<cog.count;++i) {
        *reinterpret_cast<int16_t*>(probe+24)=cog.loop;
        *reinterpret_cast<int16_t*>(probe+26)=int16_t(loop.first+i);
        motiongun::Frame frame{};if(!CogFrame(probe,joint,frame)) return false;
        if(i==0) start=frame;
        const auto x=Transform(frame.basis,{1,0,0});
        raw[i]=std::atan2(motiongun::Dot(x,Transform(start.basis,{0,0,1})),
                         motiongun::Dot(x,Transform(start.basis,{1,0,0})));
        if(firstperson::Length(Sub(frame.origin,start.origin))>16) return false;
    }
    const float end=std::fabs(raw[53]);
    if(end<firstperson::CogPegAngle*.85f || end>firstperson::CogPegAngle*1.1f) return false;
    cog.direction=raw[53]<0 ? -1.f : 1.f;cog.reference=start;
    for(int i=0;i<cog.count;++i) {
        const float angle=raw[i]*cog.direction;
        if(!std::isfinite(angle) || angle<-.01f || (i && angle<cog.angles[i-1]-.01f)) return false;
        cog.angles[i]=std::clamp(std::max(i ? cog.angles[i-1] : 0.f,angle),0.f,firstperson::CogPegAngle);
    }
    return true;
}

// Body clearance applies BEFORE grip, on the working side of this wheel only.
// The room/platform collision pass is untouched. Keep the axle/back side solid.
bool CogApproach(uint8_t* target,uint8_t* lara) {
    const uint8_t* object=nullptr;int joint=0;
    if(!PhysicalSwitchReady() || !CogObject(target,object,joint) ||
       (*reinterpret_cast<uint32_t*>(target+0x1820)&6)==6 ||
       (*reinterpret_cast<uint16_t*>(target+44)&0x100) ||
       (*reinterpret_cast<uint32_t*>(lara+0x1820)&8) ||
       *reinterpret_cast<int16_t*>(lara+off::item_hit_points)<=0) return false;
    const auto& pos=*reinterpret_cast<const PHD_3DPOS*>(target+off::item_pos);
    const auto& body=*reinterpret_cast<const PHD_3DPOS*>(lara+off::item_pos);
    if(pos.x_rot || pos.z_rot || std::abs(float(body.y_pos)-pos.y_pos)>256) return false;
    const auto local=locomotion::Rotate({float(body.x_pos)-pos.x_pos,float(body.z_pos)-pos.z_pos},-Radians(pos.y_rot));
    return std::fabs(local.x)<1024 && local.z<-256 && local.z>-1792;
}
bool CogLineOfSight(uint8_t* lara,motiongun::Vec point) {
    if(!g_motionDll || !g_motionDll->lineOfSight || !g_boundDll->getFloor) return false;
    using Floor=void* (__cdecl*)(int32_t,int32_t,int32_t,int16_t*);
    using Sight=int32_t (__cdecl*)(ShotVector*,ShotVector*);
    ShotVector eye{g_scenePose.x_pos,g_scenePose.y_pos,g_scenePose.z_pos,*reinterpret_cast<int16_t*>(lara+off::item_room),0};
    reinterpret_cast<Floor>(g_boundBase+g_boundDll->getFloor)(eye.x,eye.y,eye.z,&eye.room);
    ShotVector grip{int32_t(std::lround(point.x)),int32_t(std::lround(point.y)),int32_t(std::lround(point.z)),eye.room,0};
    return reinterpret_cast<Sight>(g_boundBase+g_motionDll->lineOfSight)(&eye,&grip)!=0;
}
bool CogContact(uint8_t* target,uint8_t* lara,motiongun::Vec hand,motiongun::Vec& local,
                motiongun::Vec& grip,float& distance) {
    const uint8_t* object=nullptr;int joint=0;
    if(!CogApproach(target,lara) || !CogObject(target,object,joint)) return false;
    const int state=*reinterpret_cast<int16_t*>(lara+18);
    if(state!=2 && state!=71 && state!=80) return false;
    motiongun::Frame frame{},inverse{};
    if(!CogFrame(target,joint,frame) || !motiongun::Inverse(frame,inverse)) return false;
    const auto point=motiongun::Transform(inverse,hand);
    local=firstperson::CogPegContact(point,*reinterpret_cast<const int16_t*>(object)==4);
    grip=motiongun::Transform(frame,local);
    const auto delta=Sub(grip,hand);distance=motiongun::Dot(delta,delta);
    const float scale=LiveWorldUnitsPerMetre();
    return std::isfinite(distance) && scale>1 && distance<scale*scale*.14f*.14f && CogLineOfSight(lara,grip);
}
float CogHandAngle(const PhysicalCog& cog,motiongun::Vec hand) {
    const auto delta=Sub(hand,cog.reference.origin);
    return cog.direction*std::atan2(motiongun::Dot(delta,Transform(cog.reference.basis,{0,0,1})),
                                  motiongun::Dot(delta,Transform(cog.reference.basis,{1,0,0})));
}
void StampCog(PhysicalCog& cog,uint8_t* target) {
    cog.animation=*reinterpret_cast<int16_t*>(target+24);cog.frame=*reinterpret_cast<int16_t*>(target+26);
}
void ShowCog(uint8_t* target) {
    SwitchPoseMatrices(target,{*reinterpret_cast<int16_t*>(target+24),*reinterpret_cast<int16_t*>(target+26),
        *reinterpret_cast<int16_t*>(target+18)});
    std::memcpy(target+6248,target+7832,1584);*reinterpret_cast<int32_t*>(target+6244)=0;
}

// Handing ownership back (native Action, mode switch, recenter) runs the native
// stop path with Lara protected. Never restore old door flags/timers/positions.
void FinishCog(PhysicalCog& cog) {
    auto* target=CogItem(cog);if(!target || !cog.started) return;
    auto* lara=*Ptr<uint8_t*>(g_boundDll->laraItem);if(!lara) return;
    CogLaraScope saved(lara);
    *Ptr<uint64_t>(g_boundDll->input)&=~uint64_t(0x40);
    *reinterpret_cast<int16_t*>(lara+24)=103;
    *reinterpret_cast<int16_t*>(target+20)=0;
    for(int i=0;i<56 && (*reinterpret_cast<uint32_t*>(target+0x1820)&6)==2;++i)
        g_hCogControl.Original<Fn_SwitchControl>()(cog.identity.index);
    ShowCog(target);
}
void ResetPhysicalCogs() {
    for(auto& cog:g_physicalCogs) FinishCog(cog);
    g_physicalCogs.clear();ReleaseCogHand();g_cogGripDown=false;
}

bool StartCog(PhysicalCog& cog,uint8_t* target,uint8_t* lara,void* collision) {
    if(cog.started) return true;
    CogLaraScope saved(lara);
    *reinterpret_cast<int16_t*>(lara+18)=2;*reinterpret_cast<int16_t*>(lara+24)=103;
    *Ptr<uint64_t>(g_boundDll->input)|=0x40;g_switchCommit=target;
    g_hCogCollision.Original<Fn_SwitchCollision>()(cog.identity.index,lara,collision);
    const int link=*reinterpret_cast<int16_t*>(target+58);
    if((*reinterpret_cast<uint32_t*>(target+0x1820)&6)!=2 || link<0 ||
       link>=*Ptr<int32_t>(g_switchDll->count)) return false;
    cog.started=true;
    // First native control changes idle->pull. Subsequent calls correspond
    // exactly to crossed pose frames, including one door pulse per cycle.
    *reinterpret_cast<int16_t*>(lara+24)=321;
    *reinterpret_cast<int16_t*>(lara+26)=(*Ptr<firstperson::SwitchAnim*>(g_boundDll->anims))[321].first;
    g_hCogControl.Original<Fn_SwitchControl>()(cog.identity.index);
    cog.phase=0;StampCog(cog,target);
    LogF("vr-cog: hand drive started item=%d door=%d",cog.identity.index,link);
    return CogItem(cog)!=nullptr;
}

void AdvancePhysicalCog(uint8_t* lara,void* collision,uint64_t now) {
    if(!g_cogClaimed) return;
    auto* cog=FindCog(g_cogGrab.index);auto* target=cog ? CogItem(*cog) : nullptr;
    motiongun::Vec hand{};float handY=0,headY=0;
    if(!cog || !target || !g_cogGripDown || !PhysicalSwitchReady() ||
       lara!=*Ptr<uint8_t*>(g_boundDll->laraItem) || !CogApproach(target,lara) ||
       !SwitchHand(hand,handY,headY) || now<g_cogLastTime || now-g_cogLastTime>250 ||
       firstperson::Length(Sub(hand,g_cogLastHand))>LiveWorldUnitsPerMetre()*.4f) {
        ReleaseCogHand();return;
    }
    g_cogLastTime=now;g_cogLastHand=hand;
    const auto contact=Add(hand,g_cogHandOffset);
    // Radial/axial motion is not a turn. Let go cleanly if the hand leaves
    // the wheel plane, rather than snapping the whole wheel back to idle.
    motiongun::Frame frame{},inverse{};
    if(!CogFrame(target,cog->joint,frame) || !motiongun::Inverse(frame,inverse)) { ReleaseCogHand();return; }
    const auto local=motiongun::Transform(inverse,contact);
    const float radius=std::hypot(g_cogLocal.x,g_cogLocal.z),trackedRadius=std::hypot(local.x,local.z);
    const float tolerance=LiveWorldUnitsPerMetre()*.22f;
    if(std::fabs(local.y)>tolerance || std::fabs(trackedRadius-radius)>tolerance || !CogLineOfSight(lara,contact)) {
        ReleaseCogHand();return;
    }
    float advance=0;
    if(!g_cogDrive.Track(CogHandAngle(*cog,contact),advance)) { ReleaseCogHand();return; }
    cog->progress+=advance;
    if(cog->progress>.01f && !StartCog(*cog,target,lara,collision)) { ReleaseCogHand();return; }
    if(cog->started) {
        CogLaraScope saved(lara);*Ptr<uint64_t>(g_boundDll->input)|=0x40;
        const auto* animations=*Ptr<firstperson::SwitchAnim*>(g_boundDll->anims);
        int budget=128;
        for(;;) {
            const bool wrap=cog->progress>=firstperson::CogPegAngle;
            const int desired=wrap ? 53 : firstperson::CogPoseAtAngle(cog->angles,cog->count,cog->progress);
            while(cog->phase<desired && budget-->0) {
                ++cog->phase;
                *reinterpret_cast<int16_t*>(lara+24)=321;
                *reinterpret_cast<int16_t*>(lara+26)=int16_t(animations[321].first+cog->phase);
                g_hCogControl.Original<Fn_SwitchControl>()(cog->identity.index);
            }
            if(!wrap || budget<=0) break;
            *reinterpret_cast<int16_t*>(lara+24)=321;*reinterpret_cast<int16_t*>(lara+26)=animations[321].first;
            g_hCogControl.Original<Fn_SwitchControl>()(cog->identity.index);--budget;
            cog->phase=0;cog->progress-=firstperson::CogPegAngle;
            const float angle=cog->direction*firstperson::CogPegAngle,c=std::cos(angle),s=std::sin(angle);
            g_cogLocal={c*g_cogLocal.x-s*g_cogLocal.z,g_cogLocal.y,s*g_cogLocal.x+c*g_cogLocal.z};
        }
        StampCog(*cog,target);ShowCog(target);
        if(!CogItem(*cog)) { ReleaseCogHand();return; }
    }
    if(CogFrame(target,cog->joint,frame)) g_cogGrip=motiongun::Transform(frame,g_cogLocal);
    if(firstperson::Length(Sub(contact,g_cogGrip))>tolerance) ReleaseCogHand();
}

bool UpdatePhysicalCogGrip(bool down,bool allowed,uint64_t now) {
    const bool pressed=down && !g_cogGripDown;g_cogGripDown=down;
    if(!down || !allowed || !g_cogDll || !PhysicalSwitchReady()) { ReleaseCogHand();return false; }
    if(g_cogClaimed && (!FindCog(g_cogGrab.index) || now<g_cogLastTime || now-g_cogLastTime>250)) ReleaseCogHand();
    if(!pressed || g_cogClaimed) return g_cogClaimed;
    auto* lara=*Ptr<uint8_t*>(g_boundDll->laraItem);
    motiongun::Vec hand{};float handY=0,headY=0;
    if(!lara || !SwitchHand(hand,handY,headY)) return false;
    auto* items=*Ptr<uint8_t*>(g_switchDll->items);const int count=*Ptr<int32_t>(g_switchDll->count);
    if(!items || count<1 || count>32768) return false;
    for(auto it=g_physicalCogs.begin();it!=g_physicalCogs.end();)
        if(!CogItem(*it)) it=g_physicalCogs.erase(it);else ++it;
    SwitchCandidate best{};motiongun::Vec bestLocal{},bestGrip{};
    for(int i=0;i<count;++i) {
        auto* target=items+i*off::item_stride;
        if((*reinterpret_cast<uint32_t*>(target+0x1820)&6) && !FindCog(i)) continue;
        float distance=0;motiongun::Vec local{},grip{};
        if(!CogContact(target,lara,hand,local,grip,distance) || (best.index>=0 && distance>=best.distance)) continue;
        best={int16_t(i),0,AppState(drva::app_off::level),items,now,distance};bestLocal=local;bestGrip=grip;
    }
    if(best.index<0) return false;
    auto* cog=FindCog(best.index);
    if(!cog) {
        PhysicalCog candidate{};if(!BuildCog(SwitchItem(best),best,candidate)) {
            LogF("vr-cog: unsupported pose path item=%d; native Action retained",best.index);return false;
        }
        g_physicalCogs.push_back(candidate);cog=&g_physicalCogs.back();
    }
    g_cogGrab=best;g_cogClaimed=true;g_cogLastTime=now;g_cogLastHand=hand;
    g_cogLocal=bestLocal;g_cogGrip=bestGrip;g_cogHandOffset=Sub(bestGrip,hand);
    g_cogDrive={};g_cogDrive.Grab(CogHandAngle(*cog,bestGrip));
    LogF("vr-cog: peg grabbed item=%d joint=%d phase=%d",best.index,cog->joint,cog->phase);
    return true;
}

void AttachPhysicalCogHand(int hand,motiongun::Frame& wrist,motiongun::Vec trackedGrip) {
    if(hand!=1 || !g_cogClaimed || !PhysicalSwitchReady() || !FindCog(g_cogGrab.index)) return;
    motiongun::Vec palm{};
    if(SwitchPalmLocal(palm)) wrist.origin=Sub(g_cogGrip,Transform(wrist.basis,palm));
    else wrist.origin=Add(wrist.origin,Sub(g_cogGrip,trackedGrip));
}

void __cdecl Detour_CogControl(int16_t index) {
    // Native door control still runs once per simulation tick. Only wheel
    // playback is held between physical pulls, so door decay remains native.
    if(FindCog(index)) return;
    g_hCogControl.Original<Fn_SwitchControl>()(index);
}
void __cdecl Detour_CogCollision(int16_t index,uint8_t* lara,void* collision) {
    const auto original=g_hCogCollision.Original<Fn_SwitchCollision>();
    if(!g_switchDll || !g_boundDll || GameDllBound()!=g_boundDll || GameDllBase()!=g_boundBase ||
       lara!=*Ptr<uint8_t*>(g_boundDll->laraItem)) { original(index,lara,collision);return; }
    auto& input=*Ptr<uint64_t>(g_boundDll->input);const auto savedInput=input;
    if((input&0x40) && !g_cogClaimed) {
        if(auto* cog=FindCog(index)) {
            FinishCog(*cog);cog->identity={};
        }
    }
    SwitchCandidate candidate{index,0,AppState(drva::app_off::level),*Ptr<uint8_t*>(g_switchDll->items),0,0};
    auto* target=SwitchItem(candidate);
    const bool relief=target && collision && CogApproach(target,lara);
    auto* flags=collision ? reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(collision)+0x8c) : nullptr;
    const uint32_t savedFlags=flags ? *flags : 0;
    if(relief) *flags&=~0x10u;
    if(g_cogClaimed && g_cogGrab.index==index) input&=~uint64_t(0x40);
    original(index,lara,collision);
    if(relief) *flags=(*flags&~0x10u)|(savedFlags&0x10u);
    input=savedInput;
}
void RemovePhysicalCogs() {
    ResetPhysicalCogs();g_hCogCollision.Remove();g_hCogControl.Remove();g_cogDll=nullptr;
}
void InstallPhysicalCogs(const GameDllLayout& d,uint64_t base) {
    if(!g_switchDll) return;
    for(const auto& row:kCogDlls) if(row.timestamp==d.timestamp) g_cogDll=&row;
    if(!g_cogDll) return;
    const uint8_t bytes[]={0x48,0x89,0x5C,0x24,0x08};
    if(!g_hCogControl.Install(reinterpret_cast<void*>(base+g_cogDll->control),reinterpret_cast<void*>(&Detour_CogControl),
            sizeof(bytes),bytes,sizeof(bytes),"VRCogControl") ||
       !g_hCogCollision.Install(reinterpret_cast<void*>(base+g_cogDll->collision),reinterpret_cast<void*>(&Detour_CogCollision),
            sizeof(bytes),bytes,sizeof(bytes),"VRCogCollision")) {
        RemovePhysicalCogs();Log("vr-cog: hook checks failed; native interaction retained");return;
    }
    Log("vr-cog: physical pegs, approach clearance and hand-driven native cog pulses enabled");
}
