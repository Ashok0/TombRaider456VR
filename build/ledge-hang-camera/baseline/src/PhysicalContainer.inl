// Searchable TR5 drawers and hinged cabinets. Native animation/state remains
// idle while independent visual joint paths are held by the player's hand.
// Both classic and remastered drawing consume these same item matrices.
struct ContainerDll { uint32_t timestamp,collision,control; };
constexpr ContainerDll kContainerDlls[]={
    {0x696B499C,0x74F90,0x74BC0}, {0x68C12FE9,0x76670,0x762A0},
};
const ContainerDll* g_containerDll=nullptr;
hook::InlineHook g_hContainerCollision,g_hContainerMatrices;
struct ContainerPose { motiongun::Frame frame{};motiongun::Vec point{}; };
struct ContainerDoor {
    int joint=0;
    motiongun::Vec local{};
    std::vector<ContainerPose> path;
    float selected=0;
};
struct PhysicalContainer {
    SwitchCandidate identity{};
    PHD_3DPOS position{};
    int16_t object=-1,animation=0,frame=0,carried=-1;
    uint32_t meshes=0,swap=0,shownMeshes=0,shownSwap=0;
    bool drawer=false,openHandle=false,searched=false;
    ContainerDoor doors[2];
};
std::vector<PhysicalContainer> g_containers;
SwitchCandidate g_containerGrab{};
bool g_containerGripDown=false,g_containerClaimed=false;
int g_containerDoor=0;
uint64_t g_containerTime=0;
motiongun::Vec g_containerGrip{},g_containerLastHand{},g_containerOffset{};

// Measured TRM handle centres, in canonical mesh coordinates. Convert through
// the native inverse bind, exactly as GetJoints does for visible vertices.
bool ContainerObject(uint8_t* item,const uint8_t*& object,motiongun::Vec handles[2],bool& openHandle) {
    if(!item || !g_containerDll || !g_switchDll || !g_boundDll || g_boundDll->game!=1 || !g_boundDll->objects) return false;
    const int slot=*reinterpret_cast<int16_t*>(item+16);
    if(slot!=164 && slot!=166) return false; // Shelves/trapped chests have no matching handles.
    object=Ptr<uint8_t>(g_boundDll->objects)+slot*off::object_stride;
    const auto* name=*reinterpret_cast<const char* const*>(object+off::object_geom);
    const int joints=*reinterpret_cast<const int16_t*>(object);
    if(!name || *reinterpret_cast<const uint64_t*>(object+56)!=g_boundBase+g_containerDll->collision ||
       *reinterpret_cast<const uint64_t*>(object+24)!=g_boundBase+g_containerDll->control ||
       *reinterpret_cast<const int32_t*>(object+off::object_geom+28)) return false;
    openHandle=false;
    if(slot==166 && joints==2) {
        if(!std::strcmp(name,"SEARCH_OBJECT2_RICH2")) handles[0]={5.22f,-160.76f,28};
        else if(!std::strcmp(name,"SEARCH_OBJECT2_ANDY1")) handles[0]={.66f,-164.5f,23};
        else if(!std::strcmp(name,"SEARCH_OBJECT2_JOBY3_JOBY5")) { handles[0]={-1.38f,-162.5f,20};openHandle=true; }
        else return false;
    } else if(slot==164 && (joints==3 || joints==8)) {
        if(!std::strcmp(name,"SEARCH_OBJECT1_RICH2")) { handles[0]={-49.96f,-1805,-228};handles[1]={49.96f,-1805,-228}; }
        else if(!std::strcmp(name,"SEARCH_OBJECT1_RICH1")) { handles[0]={-47.6f,-1814,-231};handles[1]={45.6f,-1814,-231}; }
        else if(!std::strcmp(name,"SEARCH_OBJECT1_JOBY2")) { handles[0]={-51.44f,-1804,-237};handles[1]={41.44f,-1804,-237}; }
        else if(!std::strcmp(name,"SEARCH_OBJECT1_JOBY5")) { handles[0]={-51.44f,-1801.32f,-237};handles[1]={41.44f,-1801.32f,-237}; }
        else return false;
    } else return false;
    return true;
}
uint8_t* ContainerItem(const PhysicalContainer& c) {
    auto* item=SwitchItem(c.identity);
    if(!item || *reinterpret_cast<int16_t*>(item+16)!=c.object ||
       std::memcmp(item+off::item_pos,&c.position,sizeof(c.position)) ||
       *reinterpret_cast<int16_t*>(item+24)!=c.animation || *reinterpret_cast<int16_t*>(item+26)!=c.frame ||
       *reinterpret_cast<int16_t*>(item+18)!=0 || *reinterpret_cast<int16_t*>(item+20)!=0 ||
       (*reinterpret_cast<uint32_t*>(item+0x1820)&6) || (*reinterpret_cast<uint16_t*>(item+44)&0x100) ||
       *reinterpret_cast<int16_t*>(item+50)!=c.carried ||
       *reinterpret_cast<uint32_t*>(item+12)!=c.shownMeshes ||
       *reinterpret_cast<uint32_t*>(item+0x1824)!=c.shownSwap) return nullptr;
    return item;
}
PhysicalContainer* FindContainer(int index) {
    for(auto& c:g_containers) if(c.identity.index==index && ContainerItem(c)) return &c;
    return nullptr;
}
ContainerPose ContainerAt(const ContainerDoor& door,float selected) {
    const int a=std::clamp(int(selected),0,int(door.path.size())-1),b=std::min(a+1,int(door.path.size())-1);
    const float t=std::clamp(selected-a,0.f,1.f);
    ContainerPose result=door.path[a];
    result.point=Add(result.point,motiongun::Scale(Sub(door.path[b].point,result.point),t));
    // Adjacent authored frames are at most a few degrees apart. Linear matrix
    // interpolation matches the native renderer and gives sub-frame hand motion.
    float x[12],y[12];motiongun::WriteRows(result.frame,x);motiongun::WriteRows(door.path[b].frame,y);
    for(int i=0;i<12;++i) x[i]+=(y[i]-x[i])*t;
    result.frame=motiongun::ReadRows(x);return result;
}
void ApplyContainerMatrices(PhysicalContainer& c,uint8_t* item) {
    for(int d=0;d<(c.drawer ? 1 : 2);++d) {
        const auto pose=ContainerAt(c.doors[d],c.doors[d].selected);
        motiongun::WriteRows(pose.frame,reinterpret_cast<float*>(item+7832)+c.doors[d].joint*12);
        if(c.drawer) motiongun::WriteRows(pose.frame,reinterpret_cast<float*>(item+7832)+12);
    }
    std::memcpy(item+6248,item+7832,1584);*reinterpret_cast<int32_t*>(item+6244)=0;
}
void __cdecl Detour_ContainerMatrices(uint8_t* item) {
    g_hContainerMatrices.Original<Fn_SwitchItem>()(item);
    for(auto& c:g_containers) if(ContainerItem(c)==item) { ApplyContainerMatrices(c,item);break; }
}
void ShowContainer(PhysicalContainer& c,uint8_t* item) {
    const bool open=c.doors[0].selected>.01f || (!c.drawer && c.doors[1].selected>.01f);
    c.shownMeshes=open ? (c.drawer ? (c.openHandle ? 1u : 3u) : 0xFFFFFFFFu) : c.meshes;
    c.shownSwap=(!c.drawer && open) ? 0u : c.swap;
    *reinterpret_cast<uint32_t*>(item+12)=c.shownMeshes;
    *reinterpret_cast<uint32_t*>(item+0x1824)=c.shownSwap;
    SwitchPoseMatrices(item,{c.animation,c.frame,0});ApplyContainerMatrices(c,item);
}
bool BuildContainer(uint8_t* item,SwitchCandidate identity,PhysicalContainer& c) {
    const uint8_t* object=nullptr;motiongun::Vec handles[2]{};bool openHandle=false;
    if(!ContainerObject(item,object,handles,openHandle)) return false;
    const int base=*reinterpret_cast<const int16_t*>(object+66);
    const auto* animations=*Ptr<firstperson::SwitchAnim*>(g_boundDll->anims);
    if(!animations || base<0 || *reinterpret_cast<int16_t*>(item+24)!=base ||
       *reinterpret_cast<int16_t*>(item+18)!=0 || *reinterpret_cast<int16_t*>(item+20)!=0 ||
       (*reinterpret_cast<uint32_t*>(item+0x1820)&6) || (*reinterpret_cast<uint16_t*>(item+44)&0x100)) return false;
    const bool drawer=*reinterpret_cast<int16_t*>(item+16)==166;
    const auto& idle=animations[base];const auto& use=animations[base+1];
    if(!idle.frames || !use.frames || idle.first!=idle.last || idle.state ||
       *reinterpret_cast<int16_t*>(item+26)!=idle.first ||
       use.state!=1 || use.first!=1 || use.last!=(drawer ? 200 : 313) ||
       use.jump!=base || use.jumpFrame!=idle.first || (use.interpolation&255)!=4 ||
       use.velocity || use.acceleration || use.xVelocity || use.xAcceleration) return false;
    c={};c.identity=identity;c.position=*reinterpret_cast<PHD_3DPOS*>(item+off::item_pos);
    c.object=*reinterpret_cast<int16_t*>(item+16);c.animation=int16_t(base);c.frame=*reinterpret_cast<int16_t*>(item+26);
    c.drawer=drawer;c.openHandle=openHandle;c.carried=*reinterpret_cast<int16_t*>(item+50);
    c.searched=c.carried<0;c.meshes=c.shownMeshes=*reinterpret_cast<uint32_t*>(item+12);
    c.swap=c.shownSwap=*reinterpret_cast<uint32_t*>(item+0x1824);
    alignas(16) uint8_t probe[off::item_stride];std::memcpy(probe,item,sizeof(probe));
    for(int d=0;d<(drawer ? 1 : 2);++d) {
        auto& door=c.doors[d];door.joint=drawer ? 0 : d+1;
        motiongun::Frame bind{},inverse{};
        if(!ReadIKBind(object,*reinterpret_cast<const int16_t*>(object),0,nullptr,nullptr,drawer ? 1 : door.joint,bind) ||
           !motiongun::Inverse(bind,inverse)) return false;
        door.local=motiongun::Transform(inverse,handles[d]);
        std::vector<ContainerPose> samples;
        float maximum=0;int end=0;
        for(int f=0;f<=(drawer ? 100 : 220);++f) {
            SwitchPoseMatrices(probe,{int16_t(f ? base+1 : base),int16_t(f ? f : idle.first),0});
            const auto frame=motiongun::ReadRows(reinterpret_cast<float*>(probe+7832)+door.joint*12);
            if(!firstperson::Finite(frame)) return false;
            const auto point=Add(motiongun::Transform(frame,door.local),
                {float(c.position.x_pos),float(c.position.y_pos),float(c.position.z_pos)});
            samples.push_back({frame,point});
            const float distance=firstperson::Length(Sub(point,samples[0].point));
            if(distance>maximum) { maximum=distance;end=f; }
        }
        if(maximum<(drawer ? 150 : 400) || maximum>1500) return false;
        // Remove Lara's anticipation/settling frames. Each door has its own
        // authored opening path; neither door has to wait for the other one.
        door.path.push_back(samples[0]);float previous=0;
        for(int f=1;f<=end;++f) {
            const float distance=firstperson::Length(Sub(samples[f].point,samples[0].point));
            if(distance<=previous+.2f) continue;
            door.path.push_back(samples[f]);previous=distance;
            if(distance>=maximum*.985f) break;
        }
        if(door.path.size()<2) return false;
    }
    return true;
}
void ReleaseContainerHand() { g_containerClaimed=false;g_containerGrab={}; }
void RestoreContainer(PhysicalContainer& c) {
    auto* item=ContainerItem(c);if(!item) return;
    *reinterpret_cast<uint32_t*>(item+12)=c.meshes;*reinterpret_cast<uint32_t*>(item+0x1824)=c.swap;
    c.identity={}; // Disable our matrix overlay before recalculating native idle.
    SwitchPoseMatrices(item,{c.animation,c.frame,0});
    std::memcpy(item+6248,item+7832,1584);*reinterpret_cast<int32_t*>(item+6244)=0;
}
void ResetPhysicalContainers() {
    for(auto& c:g_containers) RestoreContainer(c);
    g_containers.clear();ReleaseContainerHand();g_containerGripDown=false;
}
bool ContainerApproach(uint8_t* target,uint8_t* lara) {
    if(!PhysicalSwitchReady() || (*reinterpret_cast<uint32_t*>(lara+0x1820)&8) ||
       *reinterpret_cast<int16_t*>(lara+off::item_hit_points)<=0) return false;
    const int state=*reinterpret_cast<int16_t*>(lara+18);
    if(state!=2 && state!=71 && state!=80) return false;
    const auto& pos=*reinterpret_cast<const PHD_3DPOS*>(target+off::item_pos);
    const auto& body=*reinterpret_cast<const PHD_3DPOS*>(lara+off::item_pos);
    return !pos.x_rot && !pos.z_rot && std::abs(float(body.y_pos)-pos.y_pos)<768 &&
        std::hypot(float(body.x_pos)-pos.x_pos,float(body.z_pos)-pos.z_pos)<1800;
}
// Execute native search/pickup bookkeeping once, after opening, with Lara's
// transient animation protected. Never cache or restore the carried-item list:
// native KillItem, inventory, statistics and trophies remain authoritative.
bool SearchContainer(PhysicalContainer& c,uint8_t* target,uint8_t* lara,void* collision) {
    if(c.searched) return true;
    CogLaraScope saved(lara);
    *reinterpret_cast<uint32_t*>(target+12)=c.meshes;*reinterpret_cast<uint32_t*>(target+0x1824)=c.swap;
    *reinterpret_cast<int16_t*>(lara+18)=2;*reinterpret_cast<int16_t*>(lara+24)=103;
    *Ptr<uint64_t>(g_boundDll->input)|=0x40;g_switchCommit=target;
    g_hContainerCollision.Original<Fn_SwitchCollision>()(c.identity.index,lara,collision);
    if((*reinterpret_cast<uint32_t*>(target+0x1820)&6)!=2) { ShowContainer(c,target);return false; }
    const auto control=reinterpret_cast<Fn_SwitchControl>(g_boundBase+g_containerDll->control);
    for(int i=0;i<320 && (*reinterpret_cast<uint32_t*>(target+0x1820)&6)==2;++i) control(c.identity.index);
    c.carried=*reinterpret_cast<int16_t*>(target+50);
    c.meshes=c.shownMeshes=*reinterpret_cast<uint32_t*>(target+12);
    c.swap=c.shownSwap=*reinterpret_cast<uint32_t*>(target+0x1824);
    c.searched=c.carried<0;
    if(!ContainerItem(c)) { c.identity={};return false; }
    ShowContainer(c,target);
    LogF("vr-container: native search completed item=%d",c.identity.index);
    return c.searched;
}
void AdvancePhysicalContainer(uint8_t* lara,void* collision,uint64_t now) {
    if(!g_containerClaimed) return;
    auto* c=FindContainer(g_containerGrab.index);auto* target=c ? ContainerItem(*c) : nullptr;
    motiongun::Vec hand{};float handY=0,headY=0;const float scale=LiveWorldUnitsPerMetre();
    if(!target || !g_containerGripDown || !ContainerApproach(target,lara) ||
       !SwitchHand(hand,handY,headY) || now<g_containerTime || now-g_containerTime>250 ||
       firstperson::Length(Sub(hand,g_containerLastHand))>scale*.4f) { ReleaseContainerHand();return; }
    g_containerTime=now;g_containerLastHand=hand;
    auto& door=c->doors[g_containerDoor];const auto point=Add(hand,g_containerOffset);
    float best=1.e30f,selected=door.selected;
    for(int i=0;i+1<int(door.path.size());++i) {
        const auto a=door.path[i].point,delta=Sub(door.path[i+1].point,a);
        const float length=motiongun::Dot(delta,delta);
        if(length<.001f) continue;
        const float t=std::clamp(motiongun::Dot(Sub(point,a),delta)/length,0.f,1.f);
        const auto error=Sub(point,Add(a,motiongun::Scale(delta,t)));
        const float distance=motiongun::Dot(error,error);
        if(distance<best) { best=distance;selected=i+t; }
    }
    if(best>scale*scale*.18f*.18f || !CogLineOfSight(lara,point)) { ReleaseContainerHand();return; }
    door.selected=selected;g_containerGrip=ContainerAt(door,selected).point;ShowContainer(*c,target);
    if(selected>=float(door.path.size()-1)*.98f && !SearchContainer(*c,target,lara,collision)) ReleaseContainerHand();
}
bool UpdatePhysicalContainerGrip(bool down,bool allowed,uint64_t now) {
    const bool pressed=down && !g_containerGripDown;g_containerGripDown=down;
    if(!down || !allowed || !g_containerDll || !PhysicalSwitchReady()) { ReleaseContainerHand();return false; }
    if(g_containerClaimed && (!FindContainer(g_containerGrab.index) || now<g_containerTime || now-g_containerTime>250)) ReleaseContainerHand();
    if(!pressed || g_containerClaimed) return g_containerClaimed;
    auto* lara=*Ptr<uint8_t*>(g_boundDll->laraItem);motiongun::Vec hand{};float handY=0,headY=0;
    if(!lara || !SwitchHand(hand,handY,headY)) return false;
    auto* items=*Ptr<uint8_t*>(g_switchDll->items);const int count=*Ptr<int32_t>(g_switchDll->count);
    if(!items || count<1 || count>32768) return false;
    for(auto it=g_containers.begin();it!=g_containers.end();) if(!ContainerItem(*it)) it=g_containers.erase(it);else ++it;
    float best=LiveWorldUnitsPerMetre()*.13f;SwitchCandidate candidate{};int bestDoor=0;
    for(int i=0;i<count;++i) {
        auto* target=items+i*off::item_stride;const uint8_t* object=nullptr;motiongun::Vec handles[2]{};bool openHandle=false;
        if(!ContainerObject(target,object,handles,openHandle) || !ContainerApproach(target,lara)) continue;
        auto* c=FindContainer(i);
        if(!c) {
            PhysicalContainer next{};SwitchCandidate id{int16_t(i),0,AppState(drva::app_off::level),items,now,0};
            if(!BuildContainer(target,id,next)) continue;
            g_containers.push_back(std::move(next));c=&g_containers.back();
        }
        for(int d=0;d<(c->drawer ? 1 : 2);++d) {
            const auto point=ContainerAt(c->doors[d],c->doors[d].selected).point;
            const float distance=firstperson::Length(Sub(hand,point));
            if(distance<best && CogLineOfSight(lara,point)) { best=distance;candidate=c->identity;bestDoor=d; }
        }
    }
    if(candidate.index<0) return false;
    auto* c=FindContainer(candidate.index);if(!c) return false;
    g_containerGrab=candidate;g_containerDoor=bestDoor;g_containerClaimed=true;g_containerTime=now;
    g_containerGrip=ContainerAt(c->doors[bestDoor],c->doors[bestDoor].selected).point;
    g_containerOffset=Sub(g_containerGrip,hand);g_containerLastHand=hand;
    LogF("vr-container: handle grabbed item=%d door=%d",candidate.index,bestDoor);return true;
}
void AttachPhysicalContainerHand(int hand,motiongun::Frame& wrist,motiongun::Vec trackedGrip) {
    if(hand!=1 || !g_containerClaimed || !PhysicalSwitchReady() || !FindContainer(g_containerGrab.index)) return;
    motiongun::Vec palm{};
    if(SwitchPalmLocal(palm)) wrist.origin=Sub(g_containerGrip,Transform(wrist.basis,palm));
    else wrist.origin=Add(wrist.origin,Sub(g_containerGrip,trackedGrip));
}
void __cdecl Detour_ContainerCollision(int16_t index,uint8_t* lara,void* collision) {
    const auto original=g_hContainerCollision.Original<Fn_SwitchCollision>();
    if(!g_switchDll || !g_boundDll || GameDllBound()!=g_boundDll || GameDllBase()!=g_boundBase ||
       lara!=*Ptr<uint8_t*>(g_boundDll->laraItem)) { original(index,lara,collision);return; }
    auto& input=*Ptr<uint64_t>(g_boundDll->input);const auto saved=input;
    if(g_containerClaimed && g_containerGrab.index==index) input&=~uint64_t(0x40);
    else if(input&0x40) if(auto* c=FindContainer(index)) RestoreContainer(*c);
    original(index,lara,collision);input=saved;
}
void RemovePhysicalContainers() {
    ResetPhysicalContainers();g_hContainerCollision.Remove();g_hContainerMatrices.Remove();g_containerDll=nullptr;
}
void InstallPhysicalContainers(const GameDllLayout& d,uint64_t base) {
    if(!g_switchDll || d.game!=1) return;
    for(const auto& row:kContainerDlls) if(row.timestamp==d.timestamp) g_containerDll=&row;
    if(!g_containerDll) return;
    const uint8_t collision[]={0x48,0x89,0x5C,0x24,0x10},matrices[]={0x48,0x89,0x4C,0x24,0x08};
    const uint8_t control[]={0x40,0x53,0x57,0x41,0x56,0x41,0x57};
    if(std::memcmp(reinterpret_cast<void*>(base+g_containerDll->control),control,sizeof(control))) {
        g_containerDll=nullptr;Log("vr-container: native control check failed; native interaction retained");return;
    }
    if(!g_hContainerCollision.Install(reinterpret_cast<void*>(base+g_containerDll->collision),reinterpret_cast<void*>(&Detour_ContainerCollision),
           sizeof(collision),collision,sizeof(collision),"VRContainerCollision") ||
       !g_hContainerMatrices.Install(reinterpret_cast<void*>(base+g_switchDll->calcMatrices),reinterpret_cast<void*>(&Detour_ContainerMatrices),
           sizeof(matrices),matrices,sizeof(matrices),"VRContainerMatrices")) {
        RemovePhysicalContainers();Log("vr-container: hook checks failed; native interaction retained");return;
    }
    Log("vr-container: physical drawers and independent cabinet doors enabled");
}
