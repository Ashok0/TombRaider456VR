namespace {
tr::firstperson::SwitchAnim containerAnimations[2]{};
tr::firstperson::SwitchAnim* containerAnimationPtr=containerAnimations;
float containerPoses[221][8*12]{};
int containerJoints=0,containerActivations=0,containerPickups=0,containerControls=0;
void __cdecl FakeContainerMatrices(uint8_t* item) {
    const int frame=*reinterpret_cast<int16_t*>(item+24)==0 ? 0 : std::clamp(int(*reinterpret_cast<int16_t*>(item+26)),0,220);
    std::memcpy(item+7832,containerPoses[frame],containerJoints*48);
}
void __cdecl FakeContainerControl(int16_t) {
    ++containerControls;
    auto& frame=*reinterpret_cast<int16_t*>(switchMemory+26);
    const bool drawer=*reinterpret_cast<int16_t*>(switchMemory+16)==166;
    ++frame;
    if(frame==(drawer ? 101 : 181) && *reinterpret_cast<int16_t*>(switchMemory+50)>=0) {
        ++containerPickups;*reinterpret_cast<int16_t*>(switchMemory+50)=-1;
    }
    if(frame>(drawer ? 200 : 313)) {
        frame=0;*reinterpret_cast<int16_t*>(switchMemory+24)=0;
        *reinterpret_cast<int16_t*>(switchMemory+18)=0;
        *reinterpret_cast<uint32_t*>(switchMemory+0x1820)=0;
        *reinterpret_cast<uint32_t*>(switchMemory+12)=drawer ? 2 : 7;
        *reinterpret_cast<uint32_t*>(switchMemory+0x1824)=drawer ? 0 : 0xFFFFFFFF;
    }
}
void __cdecl FakeContainerCollision(int16_t,uint8_t* lara,void*) {
    using namespace tr;
    if(!(actionInput&0x40)) return;
    Check(*reinterpret_cast<int16_t*>(lara+18)==2 && *reinterpret_cast<int16_t*>(lara+24)==103,"container commit uses native standing precondition");
    Check(Detour_SwitchTestPosition(nullptr,switchMemory,lara)==1 && Detour_SwitchMovePosition(nullptr,switchMemory,lara)==1,
        "container native commit bypasses icon/alignment without moving player");
    ++containerActivations;
    *reinterpret_cast<int16_t*>(switchMemory+24)=1;*reinterpret_cast<int16_t*>(switchMemory+26)=2;
    *reinterpret_cast<int16_t*>(switchMemory+18)=1;*reinterpret_cast<uint32_t*>(switchMemory+0x1820)=3;
    *reinterpret_cast<int16_t*>(lara+18)=89;*reinterpret_cast<int16_t*>(lara+24)=465;
    *reinterpret_cast<int16_t*>(laraMemory+2)=1;
}
template<class Reset,class Rva,class Pose>
void TestPhysicalContainer(Reset reset,Rva rva,Pose pose) {
    using namespace tr;
    FILE* file=nullptr;fopen_s(&file,"build/container-poses.bin","rb");Check(file!=nullptr,"installed container pose fixture available");
    uint32_t cases=0;Check(std::fread(&cases,4,1,file)==1 && cases==8,"all installed drawer/cabinet variants");
    for(unsigned test=0;test<cases;++test) {
        int slot=0;char name[64]{};float bones[8][3]{};
        Check(std::fread(&slot,4,1,file)==1 && std::fread(&containerJoints,4,1,file)==1 && std::fread(name,64,1,file)==1,"read variant");
        Check(std::fread(bones,12,containerJoints,file)==size_t(containerJoints),"read inverse-bind pivots");
        for(auto& frame:containerPoses) Check(std::fread(frame,48,containerJoints,file)==size_t(containerJoints),"read authored frame matrices");
        game=dll.game=1;reset();pose(0,0,0,0);FirstPersonRecenter();
        SwitchDll table{};table.items=rva(&switchItems);table.count=rva(&switchCount);
        table.calcMatrices=rva(reinterpret_cast<void*>(&FakeContainerMatrices));
        ContainerDll containers{};containers.collision=rva(reinterpret_cast<void*>(&FakeContainerCollision));
        containers.control=rva(reinterpret_cast<void*>(&FakeContainerControl));
        MotionDll motion{};motion.lineOfSight=rva(reinterpret_cast<void*>(&FakeSwitchSight));g_motionDll=&motion;
        dll.getFloor=rva(reinterpret_cast<void*>(&FakeSwitchFloor));dll.objects=rva(leverObject);dll.anims=rva(&containerAnimationPtr);
        g_switchDll=&table;g_containerDll=&containers;
        g_hContainerCollision.m_trampoline=reinterpret_cast<void*>(&FakeContainerCollision);
        g_hContainerMatrices.m_trampoline=reinterpret_cast<void*>(&FakeContainerMatrices);
        g_hSwitchTestPosition.m_trampoline=g_hSwitchMovePosition.m_trampoline=reinterpret_cast<void*>(&FakeSwitchPosition);
        std::memset(leverObject,0,sizeof(leverObject));std::memset(switchMemory,0,sizeof(switchMemory));
        auto* object=leverObject+slot*3792;*reinterpret_cast<int16_t*>(object)=int16_t(containerJoints);
        *reinterpret_cast<Fn_SwitchControl*>(object+24)=&FakeContainerControl;
        *reinterpret_cast<Fn_SwitchCollision*>(object+56)=&FakeContainerCollision;
        *reinterpret_cast<const char**>(object+off::object_geom)=name;
        for(int j=0;j<containerJoints;++j) {
            auto* bind=reinterpret_cast<float*>(object+1676+j*64);bind[0]=bind[5]=bind[10]=bind[15]=1;
            bind[12]=-bones[j][0];bind[13]=-bones[j][1];bind[14]=-bones[j][2];
        }
        *reinterpret_cast<int16_t*>(switchMemory+16)=int16_t(slot);*reinterpret_cast<int16_t*>(switchMemory+50)=12;
        *reinterpret_cast<uint32_t*>(switchMemory+12)=slot==166 ? 2 : 7;
        *reinterpret_cast<uint32_t*>(switchMemory+0x1824)=slot==166 ? 0 : 0xFFFFFFFF;
        for(auto& a:containerAnimations){a={};a.frames=dummyLeverFrames;a.interpolation=4;}
        containerAnimations[1].state=1;containerAnimations[1].first=1;containerAnimations[1].last=slot==166 ? 200 : 313;
        containerActivations=containerPickups=containerControls=0;
        *reinterpret_cast<int16_t*>(itemMemory+24)=103;
        g_scenePose={0,-700,-100,0,0,0,0};g_scenePoseValid=true;
        VR().m_controllerPoseValid[1]=true;VR().m_rawControllerPose[1]=VR().m_rawHeadPose;
        VR().m_rawControllerPose[1].m[1][3]-=.2f;
        motiongun::Vec hand{};float handY=0,headY=0;Check(SwitchHand(hand,handY,headY),"container controller tracking");
        PhysicalContainer probe{};SwitchCandidate id{0,0,AppState(drva::app_off::level),switchItems,0,0};
        Check(BuildContainer(switchMemory,id,probe),"build native drawer/cabinet paths");
        const auto initial=probe.doors[0].path[0].point;
        auto& target=*reinterpret_cast<PHD_3DPOS*>(switchMemory+off::item_pos);
        target.x_pos=int32_t(std::lround(hand.x-initial.x));target.y_pos=int32_t(std::lround(hand.y-initial.y));target.z_pos=int32_t(std::lround(hand.z-initial.z));
        *reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos)=target;
        const auto originalController=VR().m_rawControllerPose[1];const auto originalHand=hand;
        auto setHand=[&](motiongun::Vec world) {
            const auto delta=Sub(world,originalHand);VR().m_rawControllerPose[1]=originalController;
            VR().m_rawControllerPose[1].m[0][3]+=delta.x/LiveWorldUnitsPerMetre();
            VR().m_rawControllerPose[1].m[1][3]-=delta.y/LiveWorldUnitsPerMetre();
            VR().m_rawControllerPose[1].m[2][3]-=delta.z/LiveWorldUnitsPerMetre();
        };
        uint64_t now=GetTickCount64();
        switchSightBlocked=true;
        Check(!UpdatePhysicalContainerGrip(true,true,now),"cannot grab through walls");
        UpdatePhysicalContainerGrip(false,true,++now);switchSightBlocked=false;
        game=dll.game=0;
        Check(!UpdatePhysicalContainerGrip(true,true,++now),"TR4 objects never enter TR5 container path");
        UpdatePhysicalContainerGrip(false,true,++now);game=dll.game=1;
        *reinterpret_cast<int16_t*>(laraMemory+2)=4;
        Check(!UpdatePhysicalContainerGrip(true,true,++now),"armed hand cannot grab container");
        UpdatePhysicalContainerGrip(false,true,++now);*reinterpret_cast<int16_t*>(laraMemory+2)=0;
        Check(UpdatePhysicalContainerGrip(true,true,now),"RG acquires visible handle without Action icon");
        auto* c=FindContainer(0);Check(c && g_containerDoor==0,"nearest door selected");
        auto moveTo=[&](int d,float fraction) {
            auto& door=c->doors[d];const auto point=ContainerAt(door,fraction*(door.path.size()-1)).point;
            setHand(Sub(point,g_containerOffset));AdvancePhysicalContainer(itemMemory,nullptr,++now);
        };
        AdvancePhysicalContainer(itemMemory,nullptr,++now);
        Check(containerActivations==0 && c->doors[0].selected<.01f,"holding still does not open/search");
        for(int i=1;i<=10;++i)moveTo(0,i*.025f);
        const float partial=c->doors[0].selected;
        Check(partial>1 && !containerPickups,"partial opening keeps pickup pending");
        if(slot==164) Check(c->doors[1].selected==0,"cabinet doors move independently");
        Check(*reinterpret_cast<uint32_t*>(switchMemory+12)==(slot==166 ? (c->openHandle ? 1u : 3u) : 0xFFFFFFFFu),"open mesh retains actual handle and drawer box");
        UpdatePhysicalContainerGrip(false,true,++now);
        Check(c->doors[0].selected==partial && !g_containerClaimed,"release preserves partial opening");
        float displayed[12];std::memcpy(displayed,switchMemory+7832+c->doors[0].joint*48,48);
        Detour_ContainerMatrices(switchMemory);
        Check(!std::memcmp(displayed,switchMemory+7832+c->doors[0].joint*48,48),"native redraw retains held joint pose for both renderers");
        setHand(ContainerAt(c->doors[0],partial).point);
        Check(UpdatePhysicalContainerGrip(true,true,++now),"regrab held handle");
        for(int i=11;i<=40;++i)moveTo(0,i*.025f);
        Check(containerActivations==1 && containerPickups==1 && c->searched,"opening completes native pickup exactly once");
        Check(*reinterpret_cast<int16_t*>(itemMemory+18)==2 && *reinterpret_cast<int16_t*>(itemMemory+24)==103 &&
            *reinterpret_cast<int16_t*>(laraMemory+2)==0 && actionInput==0,"search preserves body animation/gun/input state");
        Check(c->doors[0].selected>c->doors[0].path.size()-1.1f,"native search does not close physical drawer/door");
        for(int i=39;i>=0;--i)moveTo(0,i*.025f);
        Check(c->doors[0].selected<.01f && *reinterpret_cast<uint32_t*>(switchMemory+12)==c->meshes,"push handle closed restores closed mesh");
        for(int i=1;i<=40;++i)moveTo(0,i*.025f);
        Check(containerActivations==1 && containerPickups==1,"close and reopen never duplicates pickup");
        UpdatePhysicalContainerGrip(false,true,++now);
        if(slot==164) {
            setHand(c->doors[1].path[0].point);
            Check(UpdatePhysicalContainerGrip(true,true,++now) && g_containerDoor==1,"other cabinet handle can be grabbed independently");
            for(int i=1;i<=40;++i)moveTo(1,i*.025f);
            Check(c->doors[0].selected>c->doors[0].path.size()-1.1f && c->doors[1].selected>c->doors[1].path.size()-1.1f,"both cabinet doors remain open");
            Check(containerPickups==1,"opening second door cannot duplicate loot");
        }
        const float held=c->doors[g_containerDoor].selected;
        setHand(Add(g_containerGrip,{0,LiveWorldUnitsPerMetre()*.5f,0}));AdvancePhysicalContainer(itemMemory,nullptr,++now);
        Check(!g_containerClaimed && c->doors[g_containerDoor].selected==held,"tracking jump releases without snapping back");
        ResetPhysicalContainers();Check(g_containers.empty() && *reinterpret_cast<int16_t*>(switchMemory+50)==-1,"reset clears visual pose without resurrecting loot");
        Check(*reinterpret_cast<uint32_t*>(switchMemory+12)==(slot==166 ? 2u : 7u),"reset restores native closed masks");
        // Native Action takes ownership from an open visual pose cleanly.
        Check(BuildContainer(switchMemory,id,probe),"native fallback fixture");
        g_containers.push_back(probe);c=&g_containers.back();c->doors[0].selected=2;
        ShowContainer(*c,switchMemory);actionInput=0x40;
        // Pretend the conventional native Action is already inside its own
        // alignment scope, so the stand-in can verify restoration at entry.
        g_switchCommit=switchMemory;
        Detour_ContainerCollision(0,itemMemory,nullptr);
        Check(!FindContainer(0) && *reinterpret_cast<int16_t*>(switchMemory+24)==1,"native Action restores idle before normal search takes over");
        Check(actionInput==0x40,"collision preserves conventional Action input");
        while((*reinterpret_cast<uint32_t*>(switchMemory+0x1820)&6)==2)FakeContainerControl(0);
        actionInput=0;g_switchCommit=nullptr;ResetPhysicalContainers();
        // Stale level/item identity must not overwrite a reused native slot.
        Check(BuildContainer(switchMemory,id,probe),"rebuild after search");g_containers.push_back(probe);
        ++*reinterpret_cast<int16_t*>(switchMemory+16);ResetPhysicalContainers();
        Check(*reinterpret_cast<int16_t*>(switchMemory+16)==slot+1,"stale object remains untouched");
        g_containerDll=nullptr;g_switchDll=nullptr;g_motionDll=nullptr;
    }
    std::fclose(file);
}
}
