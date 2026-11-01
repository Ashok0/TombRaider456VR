// The native PULLEY family is a chain in TR4 and a rope in TR5. Other
// CHAIN/ROPE slots are traps or traversal objects and must never enter here.
struct PulleyDll { uint32_t timestamp,collision,laraControl,control,switchTrigger; };
constexpr PulleyDll kPulleyDlls[]={
    {0x696B4999,0x8BFD0,0x53D10,0x8B760,0x8B640},
    {0x696B499C,0x8B200,0x524B0,0x8A770,0x8A5C0},
    {0x68C12FDA,0x8C9E0,0x53A10,0x8C170,0x8C060},
    {0x68C12FE9,0x8B510,0x531C0,0x8AA80,0x8A8E0},
};
const PulleyDll* g_pulleyDll=nullptr;
hook::InlineHook g_hPulleyCollision,g_hPulleyControl;
struct PhysicalPulley {
    SwitchCandidate identity{};
    PHD_3DPOS position{};
    int16_t object=-1,animation=0,frame=0,originalFrame=0,remaining=0,setting=0,latched=0;
    int count=0;
    float offsets[73]{},progress=0,stroke=0;
    bool complete=false;
    uint64_t time=0;
};
std::vector<PhysicalPulley> g_physicalPulleys;
SwitchCandidate g_pulleyGrab{};
bool g_pulleyGripDown=false,g_pulleyClaimed=false;
uint64_t g_pulleyLastTime=0;
float g_pulleyStartProgress=0;
float g_pulleyStartHeight=0,g_pulleyLastHeight=0;
motiongun::Vec g_pulleyStartHand{},g_pulleyLastHand{},g_pulleyGrip{};

bool PulleyObject(uint8_t* item,const uint8_t*& object) {
    if(!item || !g_pulleyDll || !g_switchDll || !g_boundDll || !g_boundDll->objects || g_boundDll->game>1) return false;
    const int slot=g_boundDll->game==0 ? 321 : 282;
    if(*reinterpret_cast<int16_t*>(item+16)!=slot) return false;
    object=Ptr<uint8_t>(g_boundDll->objects)+slot*off::object_stride;
    const auto* name=*reinterpret_cast<const char* const*>(object+off::object_geom);
    return *reinterpret_cast<const uint64_t*>(object+56)==g_boundBase+g_pulleyDll->collision &&
        *reinterpret_cast<const uint64_t*>(object+24)==g_boundBase+g_pulleyDll->control &&
        *reinterpret_cast<const int16_t*>(object)==(g_boundDll->game==0 ? 9 : 1) &&
        !*reinterpret_cast<const int32_t*>(object+off::object_geom+28) && name && !std::strcmp(name,"PULLEY");
}
bool PulleyUsable(uint8_t* item) {
    // TR4's locked chain and TR5's special GlobalPulleyFrigItem stay native.
    // Do not clear their locks or guess their level-specific completion rules.
    return !(*reinterpret_cast<uint16_t*>(item+44)&0x100) &&
        !*reinterpret_cast<int16_t*>(item+58) && *reinterpret_cast<int16_t*>(item+48)>0 &&
        (*reinterpret_cast<uint32_t*>(item+0x1820)&6)!=6;
}
uint8_t* PulleyItem(const PhysicalPulley& p) {
    auto* item=SwitchItem(p.identity);const uint8_t* object=nullptr;
    if(!item || !PulleyObject(item,object) || *reinterpret_cast<int16_t*>(item+16)!=p.object ||
       std::memcmp(item+off::item_pos,&p.position,sizeof(p.position)) ||
       *reinterpret_cast<int16_t*>(item+18)!=0 || *reinterpret_cast<int16_t*>(item+24)!=p.animation ||
       *reinterpret_cast<int16_t*>(item+26)!=p.frame || *reinterpret_cast<int16_t*>(item+48)!=p.remaining ||
       *reinterpret_cast<int16_t*>(item+62)!=p.setting || *reinterpret_cast<int16_t*>(item+60)!=p.latched) return nullptr;
    return item;
}
PhysicalPulley* FindPulley(int index) {
    for(auto& p:g_physicalPulleys) if(p.identity.index==index && PulleyItem(p)) return &p;
    return nullptr;
}
void ReleasePulleyHand() { g_pulleyClaimed=false;g_pulleyGrab={}; }
void ResetPhysicalPulleys() {
    for(auto& p:g_physicalPulleys) if(auto* item=PulleyItem(p)) {
        *reinterpret_cast<int16_t*>(item+26)=p.originalFrame;ShowCog(item);
    }
    g_physicalPulleys.clear();ReleasePulleyHand();g_pulleyGripDown=false;
}

bool PulleyApproach(uint8_t* target,uint8_t* lara) {
    const uint8_t* object=nullptr;
    if(!PhysicalSwitchReady() || !lara || !PulleyObject(target,object) || !PulleyUsable(target) ||
       (*reinterpret_cast<uint32_t*>(lara+0x1820)&8) || *reinterpret_cast<int16_t*>(lara+off::item_hit_points)<=0) return false;
    const int state=*reinterpret_cast<int16_t*>(lara+18);
    if(state!=2 && state!=71 && state!=80) return false;
    const auto& pos=*reinterpret_cast<const PHD_3DPOS*>(target+off::item_pos);
    const auto& body=*reinterpret_cast<const PHD_3DPOS*>(lara+off::item_pos);
    return !pos.x_rot && !pos.z_rot && std::abs(float(body.y_pos)-pos.y_pos)<256 &&
        std::hypot(float(body.x_pos)-pos.x_pos,float(body.z_pos)-pos.z_pos)<LiveWorldUnitsPerMetre()*1.2f;
}

// Require the real switch trigger under Lara. A reachable rope across a floor
// boundary must not queue a mechanism for activation later in another room.
bool PulleyAtLara(int16_t index,uint8_t* lara) {
    alignas(16) uint8_t probe[off::item_stride];std::memcpy(probe,lara,sizeof(probe));
    int16_t links[1024]{};
    using Query=int32_t (__cdecl*)(uint8_t*,int16_t*);
    const int count=reinterpret_cast<Query>(g_boundBase+g_pulleyDll->switchTrigger)(probe,links);
    if(count<1 || count>1024) return false;
    for(int i=0;i<count;++i) if(links[i]==index) return true;
    return false;
}

bool BuildPulley(uint8_t* item,SwitchCandidate identity,PhysicalPulley& p,uint64_t now) {
    const uint8_t* object=nullptr;
    if(!PulleyObject(item,object) || !PulleyUsable(item) || *reinterpret_cast<int16_t*>(item+18)!=0 ||
       (*reinterpret_cast<uint32_t*>(item+0x1820)&6)==4) return false;
    const int base=*reinterpret_cast<const int16_t*>(object+66);
    const auto* animations=*Ptr<firstperson::SwitchAnim*>(g_boundDll->anims);
    if(base<0 || !animations || *reinterpret_cast<int16_t*>(item+24)!=base) return false;
    const auto& a=animations[base];const auto& pull=animations[340];
    const bool chain=g_boundDll->game==0;
    const int size=a.interpolation>>8,rate=a.interpolation&255;
    if(!a.frames || a.state || a.first!=0 || a.last!=(chain ? 72 : 58) || a.jump!=base ||
       a.jumpFrame!=(chain ? 15 : 0) || (rate!=1 && !(chain && rate==2)) || size!=(chain ? 19 : 11) ||
       a.changeCount || a.commandCount || a.velocity || a.acceleration || a.xVelocity || a.xAcceleration ||
       !pull.frames || pull.state!=104 || pull.last-pull.first!=58 ||
       !animations[339].frames || animations[339].state!=104 ||
       *reinterpret_cast<int16_t*>(item+26)<a.first || *reinterpret_cast<int16_t*>(item+26)>a.last) return false;
    p={};p.identity=identity;p.position=*reinterpret_cast<PHD_3DPOS*>(item+off::item_pos);
    p.object=*reinterpret_cast<int16_t*>(item+16);p.animation=int16_t(base);
    p.frame=p.originalFrame=*reinterpret_cast<int16_t*>(item+26);p.remaining=*reinterpret_cast<int16_t*>(item+48);
    p.setting=*reinterpret_cast<int16_t*>(item+62);p.latched=*reinterpret_cast<int16_t*>(item+60);
    if(!p.setting || p.setting==INT16_MIN || p.remaining>std::abs(int(p.setting))) return false;
    p.count=a.last+1;p.stroke=LiveWorldUnitsPerMetre()*.35f;p.time=now;
    if(!std::isfinite(p.stroke) || p.stroke<20 || p.stroke>450) return false;
    // Verify the actual root path. TR4 translates down; TR5's rope is static
    // even in the original animation, so the palm slides along its visible axis.
    for(int i=0;i<p.count;++i) {
        const int key=i/rate,next=std::min(key+1,a.last/rate);
        const auto* f=a.frames+key*size;const auto* n=a.frames+next*size;
        const float fraction=float(i%rate)/rate;
        p.offsets[i]=(f[7]+fraction*(n[7]-f[7]))-a.frames[7];
        if(f[6]!=a.frames[6] || f[8]!=a.frames[8] ||
           std::memcmp(f+9,a.frames+9,(size-9)*sizeof(int16_t)) ||
           !std::isfinite(p.offsets[i]) || p.offsets[i]<0 || (i && p.offsets[i]<p.offsets[i-1])) return false;
    }
    if(chain ? std::fabs(p.offsets[p.count-1]-507)>1 : p.offsets[p.count-1]!=0) return false;
    // A native animation saved mid-pull returns gently before the first grab.
    // Its visual pose never counts as a fresh physical stroke.
    p.progress=p.offsets[p.frame];p.complete=p.progress>0;
    return true;
}
void ShowPulley(PhysicalPulley& p,uint8_t* item) {
    int best=0;
    if(g_boundDll->game==0) for(int i=1;i<p.count;++i)
        if(std::fabs(p.offsets[i]-p.progress)<std::fabs(p.offsets[best]-p.progress)) best=i;
    p.frame=int16_t(best);*reinterpret_cast<int16_t*>(item+26)=p.frame;ShowCog(item);
}

bool PulleyContact(uint8_t* item,uint8_t* lara,motiongun::Vec hand,motiongun::Vec& grip,float& distance) {
    const uint8_t* object=nullptr;
    if(!PulleyApproach(item,lara) || !PulleyObject(item,object)) return false;
    alignas(16) uint8_t probe[off::item_stride];std::memcpy(probe,item,sizeof(probe));
    SwitchPoseMatrices(probe,{*reinterpret_cast<int16_t*>(item+24),*reinterpret_cast<int16_t*>(item+26),0});
    const auto& pos=*reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos);
    const int joints=*reinterpret_cast<const int16_t*>(object);distance=1e30f;
    // Bounds measured from PULLEY.TRM, in canonical mesh coordinates.
    constexpr float low[]={-246.42f,-503.42f,-758.42f,-1015.42f,-1270.42f,-1525.42f,-1782.42f,-2039.42f,-2066.37f};
    constexpr float high[]={16.37f,-240.63f,-495.63f,-752.63f,-1007.63f,-1262.63f,-1519.63f,-1776.63f,-2033.63f};
    for(int j=0;j<joints;++j) {
        motiongun::Frame bind{},inverseBind{},inverse{};
        if(!ReadIKBind(object,joints,0,nullptr,nullptr,j,bind) || !motiongun::Inverse(bind,inverseBind)) return false;
        auto frame=motiongun::Multiply(motiongun::ReadRows(reinterpret_cast<float*>(probe+7832)+j*12),inverseBind);
        frame.origin=Add(frame.origin,{float(pos.x_pos),float(pos.y_pos),float(pos.z_pos)});
        if(!firstperson::Finite(frame) || !motiongun::Inverse(frame,inverse)) return false;
        auto local=motiongun::Transform(inverse,hand);
        const float radius=joints==9 ? 16.f : 7.f,r=std::hypot(local.x,local.y);
        if(r>radius) { local.x*=radius/r;local.y*=radius/r; }
        local.z=std::clamp(local.z,joints==9 ? low[j] : -1538.75f,joints==9 ? high[j] : 510.25f);
        const auto point=motiongun::Transform(frame,local);const auto delta=Sub(point,hand);
        const float d=motiongun::Dot(delta,delta);
        if(d<distance) { distance=d;grip=point; }
    }
    const float reach=LiveWorldUnitsPerMetre()*.12f;
    return std::isfinite(distance) && distance<reach*reach && CogLineOfSight(lara,grip);
}

bool CommitPulley(PhysicalPulley& p,uint8_t* target,uint8_t* lara) {
    if(!PulleyUsable(target) || (*reinterpret_cast<uint32_t*>(target+0x1820)&6)==4 ||
       !PulleyAtLara(p.identity.index,lara)) return false;
    // No fast-forwarded animation, linked-door writes, or fabricated counters.
    // Native collision registers the active item/achievements. The verified
    // Lara control function applies exactly one +44 pull event, including its
    // multi-pull, resettable and one-use rules. Normal floor triggers follow.
    CogLaraScope saved(lara);
    const int32_t interaction=*Ptr<int32_t>(g_boundDll->lara+0x15A);
    RoomCollision collision{};
    *reinterpret_cast<int16_t*>(lara+18)=2;*reinterpret_cast<int16_t*>(lara+24)=103;
    *Ptr<uint64_t>(g_boundDll->input)|=0x40;g_switchCommit=target;
    g_hPulleyCollision.Original<Fn_SwitchCollision>()(p.identity.index,lara,&collision);
    const bool started=(*reinterpret_cast<uint32_t*>(target+0x1820)&6)==2 &&
        *reinterpret_cast<int16_t*>(lara+18)==104 && *Ptr<int16_t>(g_boundDll->lara+0x15A)==p.identity.index;
    if(started) {
        *reinterpret_cast<int16_t*>(lara+24)=340;
        *reinterpret_cast<int16_t*>(lara+26)=int16_t((*Ptr<firstperson::SwitchAnim*>(g_boundDll->anims))[340].first+44);
        using Control=void (__cdecl*)(uint8_t*,void*);
        reinterpret_cast<Control>(g_boundBase+g_pulleyDll->laraControl)(lara,&collision);
    }
    *Ptr<int32_t>(g_boundDll->lara+0x15A)=interaction;
    p.remaining=*reinterpret_cast<int16_t*>(target+48);p.latched=*reinterpret_cast<int16_t*>(target+60);
    if(started) LogF("vr-pulley: physical pull item=%d remaining=%d latch=%d",p.identity.index,p.remaining,p.latched);
    return started;
}

void AdvancePhysicalPulley(uint8_t* lara,void*,uint64_t now) {
    if(!PhysicalSwitchReady()) { ResetPhysicalPulleys();return; }
    for(auto it=g_physicalPulleys.begin();it!=g_physicalPulleys.end();) {
        auto* item=PulleyItem(*it);
        if(!item) { it=g_physicalPulleys.erase(it);continue; }
        if(it->complete && (!g_pulleyClaimed || g_pulleyGrab.index!=it->identity.index)) {
            const float dt=now>=it->time ? std::min(float(now-it->time)*.001f,.1f) : 0;
            it->progress=std::max(0.f,it->progress-LiveWorldUnitsPerMetre()*1.4f*dt);
            ShowPulley(*it,item);if(it->progress==0) it->complete=false;
        }
        it->time=now;++it;
    }
    if(!g_pulleyClaimed) return;
    auto* p=FindPulley(g_pulleyGrab.index);auto* target=p ? PulleyItem(*p) : nullptr;
    motiongun::Vec hand{};float handY=0,headY=0;
    if(!p || !target || !g_pulleyGripDown || lara!=*Ptr<uint8_t*>(g_boundDll->laraItem) ||
       !PulleyApproach(target,lara) || !SwitchHand(hand,handY,headY) ||
       !std::isfinite(hand.x) || !std::isfinite(hand.y) || !std::isfinite(hand.z) || now<g_pulleyLastTime ||
       now-g_pulleyLastTime>250 || firstperson::Length(Sub(hand,g_pulleyLastHand))>LiveWorldUnitsPerMetre()*.4f ||
       std::fabs(handY-g_pulleyLastHeight)>.4f ||
       std::hypot(hand.x-g_pulleyStartHand.x,hand.z-g_pulleyStartHand.z)>LiveWorldUnitsPerMetre()*.22f) {
        ReleasePulleyHand();return;
    }
    g_pulleyLastTime=now;g_pulleyLastHand=hand;g_pulleyLastHeight=handY;
    if(!p->complete) {
        // Net physical downward travel, not accumulated positive deltas or
        // virtual body/elevator movement. Shaking cannot manufacture pulls.
        p->progress=std::clamp(g_pulleyStartProgress+(g_pulleyStartHeight-handY)*LiveWorldUnitsPerMetre(),0.f,p->stroke);
        ShowPulley(*p,target);
    }
    float distance=0;
    if(!PulleyContact(target,lara,hand,g_pulleyGrip,distance)) { ReleasePulleyHand();return; }
    if(!p->complete && p->progress>=p->stroke-.01f) {
        p->complete=true;
        if(!CommitPulley(*p,target,lara)) ReleasePulleyHand();
    }
}

bool UpdatePhysicalPulleyGrip(bool down,bool allowed,uint64_t now) {
    const bool pressed=down && !g_pulleyGripDown;g_pulleyGripDown=down;
    if(!down || !allowed || !g_pulleyDll || !PhysicalSwitchReady()) { ReleasePulleyHand();return false; }
    if(g_pulleyClaimed && (!FindPulley(g_pulleyGrab.index) || now<g_pulleyLastTime || now-g_pulleyLastTime>250)) ReleasePulleyHand();
    if(!pressed || g_pulleyClaimed) return g_pulleyClaimed;
    auto* lara=*Ptr<uint8_t*>(g_boundDll->laraItem);motiongun::Vec hand{};float handY=0,headY=0;
    if(!lara || !SwitchHand(hand,handY,headY)) return false;
    auto* items=*Ptr<uint8_t*>(g_switchDll->items);const int count=*Ptr<int32_t>(g_switchDll->count);
    if(!items || count<1 || count>32768) return false;
    SwitchCandidate best{};motiongun::Vec bestGrip{};
    for(int i=0;i<count;++i) {
        auto* target=items+i*off::item_stride;float distance=0;motiongun::Vec grip{};
        if(!PulleyContact(target,lara,hand,grip,distance) || (best.index>=0 && distance>=best.distance) ||
           (*reinterpret_cast<uint32_t*>(target+0x1820)&6)==4 || !PulleyAtLara(int16_t(i),lara)) continue;
        best={int16_t(i),0,AppState(drva::app_off::level),items,now,distance};bestGrip=grip;
    }
    if(best.index<0) return false;
    auto* p=FindPulley(best.index);
    if(!p) {
        PhysicalPulley candidate{};if(!BuildPulley(SwitchItem(best),best,candidate,now)) return false;
        g_physicalPulleys.push_back(candidate);p=&g_physicalPulleys.back();
    }
    if(p->complete) return false;
    g_pulleyGrab=best;g_pulleyClaimed=true;g_pulleyLastTime=now;
    g_pulleyStartHand=g_pulleyLastHand=hand;g_pulleyStartProgress=p->progress;g_pulleyGrip=bestGrip;
    g_pulleyStartHeight=g_pulleyLastHeight=handY;
    return true;
}
void AttachPhysicalPulleyHand(int hand,motiongun::Frame& wrist,motiongun::Vec trackedGrip) {
    if(hand!=1 || !g_pulleyClaimed || !PhysicalSwitchReady() || !FindPulley(g_pulleyGrab.index)) return;
    motiongun::Vec palm{};
    if(SwitchPalmLocal(palm)) wrist.origin=Sub(g_pulleyGrip,Transform(wrist.basis,palm));
    else wrist.origin=Add(wrist.origin,Sub(g_pulleyGrip,trackedGrip));
}

void __cdecl Detour_PulleyControl(int16_t index) {
    auto* p=FindPulley(index);auto* item=p ? PulleyItem(*p) : nullptr;
    // SwitchControl is shared. Always run it exactly once so timers, flags and
    // every other switch keep native behavior. Only our command-free pose is held.
    g_hPulleyControl.Original<Fn_SwitchControl>()(index);
    if(item && *reinterpret_cast<int16_t*>(item+24)==p->animation && *reinterpret_cast<int16_t*>(item+18)==0) {
        *reinterpret_cast<int16_t*>(item+26)=p->frame;ShowCog(item);
    }
}
void __cdecl Detour_PulleyCollision(int16_t index,uint8_t* lara,void* collision) {
    const auto original=g_hPulleyCollision.Original<Fn_SwitchCollision>();
    if(!g_pulleyDll || !g_boundDll || GameDllBound()!=g_boundDll || GameDllBase()!=g_boundBase ||
       lara!=*Ptr<uint8_t*>(g_boundDll->laraItem)) { original(index,lara,collision);return; }
    auto& input=*Ptr<uint64_t>(g_boundDll->input);const auto saved=input;
    if(g_pulleyClaimed) input&=~uint64_t(0x40);
    else if(input&0x40) {
        // Explicit native Action takes over without rolling back counters.
        if(auto* p=FindPulley(index)) {
            if(auto* item=PulleyItem(*p)) { *reinterpret_cast<int16_t*>(item+26)=p->originalFrame;ShowCog(item); }
            p->identity={};
        }
    }
    original(index,lara,collision);input=saved;
}
void RemovePhysicalPulleys() {
    ResetPhysicalPulleys();g_hPulleyCollision.Remove();g_hPulleyControl.Remove();g_pulleyDll=nullptr;
}
void InstallPhysicalPulleys(const GameDllLayout& d,uint64_t base) {
    if(!g_switchDll) return;
    for(const auto& row:kPulleyDlls) if(row.timestamp==d.timestamp) g_pulleyDll=&row;
    if(!g_pulleyDll) return;
    const uint8_t collision[]={0x48,0x89,0x5c,0x24,uint8_t(d.game==0 ? 0x18 : 0x10)};
    const uint8_t control4[]={0x48,0x0f,0xbf,0xc1,0x45,0x33,0xd2};
    const uint8_t control5[]={0x48,0x89,0x5c,0x24,0x18};
    const auto* control=d.game==0 ? control4 : control5;const size_t size=d.game==0 ? sizeof(control4) : sizeof(control5);
    if(!g_hPulleyControl.Install(reinterpret_cast<void*>(base+g_pulleyDll->control),reinterpret_cast<void*>(&Detour_PulleyControl),
            size,control,size,"VRPulleyControl") ||
       !g_hPulleyCollision.Install(reinterpret_cast<void*>(base+g_pulleyDll->collision),reinterpret_cast<void*>(&Detour_PulleyCollision),
            sizeof(collision),collision,sizeof(collision),"VRPulleyCollision")) {
        RemovePhysicalPulleys();Log("vr-pulley: signature mismatch; native interaction retained");return;
    }
    Log("vr-pulley: physical RG pulls enabled for verified mechanism chains/ropes");
}
