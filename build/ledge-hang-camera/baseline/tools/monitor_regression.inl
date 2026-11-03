namespace {
tr::firstperson::SwitchAnim monitorAnimations[198]{};
tr::firstperson::SwitchAnim* monitorAnimationPtr=monitorAnimations;
int16_t monitorFrames[]={-202,205,-924,-223,358,516,0,-223,0,int16_t(0x8800)};
int monitorActivations=0,monitorTriggers=0;
bool monitorTriggerChangesLara=false,monitorRefuse=false;
void __cdecl FakeMonitorMatrices(uint8_t* item) {
    using namespace tr;
    const float angle=Radians(reinterpret_cast<const PHD_3DPOS*>(item+off::item_pos)->y_rot)+kPi;
    const float c=std::cos(angle),s=std::sin(angle);
    motiongun::WriteRows({{{{c,0,s},{0,1,0},{-s,0,c}}},{0,-223,0}},reinterpret_cast<float*>(item+7832));
}
void __cdecl FakeMonitorCollision(int16_t index,uint8_t* lara,void*) {
    using namespace tr;
    Check(index==0 && (actionInput&0x40) && *reinterpret_cast<int16_t*>(lara+18)==2 && *reinterpret_cast<int16_t*>(lara+24)==103,
        "screen touch supplies native monitor Action/standing precondition");
    Check(Detour_SwitchTestPosition(nullptr,switchMemory,lara)==1 && Detour_SwitchMovePosition(nullptr,switchMemory,lara)==1,
        "screen touch needs no icon or body auto-alignment");
    if(monitorRefuse) return;
    ++monitorActivations;
    *reinterpret_cast<uint32_t*>(switchMemory+0x1820)|=2;
    *reinterpret_cast<uint16_t*>(switchMemory+44)|=0x20;
    *reinterpret_cast<int16_t*>(lara+18)=40;*reinterpret_cast<int16_t*>(lara+24)=197;
    *reinterpret_cast<int16_t*>(laraMemory+2)=1;
    *reinterpret_cast<int32_t*>(laraMemory+224)=1234;
}
void __cdecl FakeMonitorTrigger(int32_t x,int32_t y,int32_t z,int16_t room,int16_t heavy,int16_t flags) {
    using namespace tr;
    ++monitorTriggers;
    const auto& pos=*reinterpret_cast<PHD_3DPOS*>(switchMemory+off::item_pos);
    Check(x==pos.x_pos && y==pos.y_pos && z==pos.z_pos && room==7 && heavy==1 && flags==0,"screen touch uses native monitor floor trigger arguments");
    Check(*reinterpret_cast<int16_t*>(itemMemory+18)==2 && *reinterpret_cast<int16_t*>(itemMemory+24)==103 &&
          *reinterpret_cast<int16_t*>(laraMemory+2)==0 && *reinterpret_cast<int32_t*>(laraMemory+224)==0 && actionInput==0 && !g_switchCommit,
          "Lara and input restored before level trigger");
    if(monitorTriggerChangesLara) {
        *reinterpret_cast<int16_t*>(itemMemory+18)=99;*reinterpret_cast<int16_t*>(itemMemory+24)=480;
    }
}
template<class Reset,class Rva,class Pose>
void TestPhysicalMonitor(Reset reset,Rva rva,Pose pose) {
    using namespace tr;
    using firstperson::MonitorScreenTouch;
    Check(MonitorScreenTouch({0,-500,-300},{0,-500,-390},1024),"front screen touch accepted");
    Check(!MonitorScreenTouch({0,-500,-500},{0,-500,-390},1024),"back of television does not activate");
    Check(!MonitorScreenTouch({0,-500,-390},{0,-500,-390},1024),"resting hand cannot repeat");
    Check(!MonitorScreenTouch({0,-500,-390},{0,-500,-300},1024),"withdrawal does not activate");
    Check(!MonitorScreenTouch({220,-500,-300},{220,-500,-390},1024),"side bezel is not glass");
    Check(!MonitorScreenTouch({0,-200,-300},{0,-200,-390},1024),"stand is not glass");
    Check(!MonitorScreenTouch({0,-800,-300},{0,-800,-390},1024),"top casing is not glass");
    Check(!MonitorScreenTouch({0,-500,1000},{0,-500,-390},1024),"tracking teleport cannot touch");
    Check(!MonitorScreenTouch({0,-500,-300},{0,-500,-600},1024),"deep penetration cannot touch");
    Check(!MonitorScreenTouch({NAN,-500,-300},{0,-500,-390},1024),"non-finite tracking rejected");
    for(int hand=0;hand<2;++hand) for(int angle=0;angle<4;++angle) for(int dummy=0;dummy<2;++dummy) {
        game=dll.game=1;reset();pose(0,0,0,0);FirstPersonRecenter();
        SwitchDll table{};table.items=rva(&switchItems);table.count=rva(&switchCount);
        table.calcMatrices=rva(reinterpret_cast<void*>(&FakeMonitorMatrices));
        MonitorDll monitor{};monitor.collision=rva(reinterpret_cast<void*>(&FakeMonitorCollision));
        monitor.trigger=rva(reinterpret_cast<void*>(&FakeMonitorTrigger));g_monitorDll=&monitor;
        MotionDll motion{};motion.lineOfSight=rva(reinterpret_cast<void*>(&FakeSwitchSight));g_motionDll=&motion;
        dll.getFloor=rva(reinterpret_cast<void*>(&FakeSwitchFloor));dll.objects=rva(leverObject);dll.anims=rva(&monitorAnimationPtr);
        g_switchDll=&table;
        g_hSwitchTestPosition.m_trampoline=g_hSwitchMovePosition.m_trampoline=reinterpret_cast<void*>(&FakeSwitchPosition);
        std::memset(leverObject,0,sizeof(leverObject));std::memset(switchMemory,0,sizeof(switchMemory));
        auto* object=leverObject+151*3792;*reinterpret_cast<int16_t*>(object)=1;
        *reinterpret_cast<Fn_SwitchCollision*>(object+56)=&FakeMonitorCollision;
        *reinterpret_cast<const char**>(object+off::object_geom)=dummy ? "DUMMY" : "MONITOR_SCREEN";
        auto* bind=reinterpret_cast<float*>(object+1676);bind[0]=bind[5]=bind[10]=bind[15]=1;
        *reinterpret_cast<int16_t*>(leverObject)=15;
        for(int j:{10,13}) {
            auto* palm=reinterpret_cast<float*>(leverObject+1676+j*64);palm[0]=palm[5]=palm[10]=palm[15]=1;
        }
        *reinterpret_cast<int16_t*>(switchMemory+16)=151;*reinterpret_cast<int16_t*>(switchMemory+28)=7;
        monitorAnimations[0]={};monitorAnimations[0].frames=monitorFrames;monitorAnimations[0].interpolation=2561;
        monitorActivations=monitorTriggers=0;monitorRefuse=monitorTriggerChangesLara=false;
        *reinterpret_cast<int16_t*>(itemMemory+24)=103;g_scenePose={0,-700,-100,0,0,0,0};g_scenePoseValid=true;
        VR().m_controllerPoseValid[hand]=true;VR().m_controllerPoseValid[1-hand]=false;
        VR().m_rawControllerPose[hand]=VR().m_rawHeadPose;VR().m_rawControllerPose[hand].m[1][3]-=.2f;
        motiongun::Vec initialHand{};Check(MonitorHand(hand,initialHand),"tracked left/right palm available");
        auto& target=*reinterpret_cast<PHD_3DPOS*>(switchMemory+off::item_pos);target.y_rot=int16_t(angle*16384);
        motiongun::Frame frame{},inverse{};Check(MonitorFrame(switchMemory,frame,inverse),"known monitor with visible or dummy HD mesh accepted");
        const auto initial=motiongun::Transform(frame,{0,-500,-300});
        target.x_pos=int32_t(std::lround(initialHand.x-initial.x));target.y_pos=int32_t(std::lround(initialHand.y-initial.y));target.z_pos=int32_t(std::lround(initialHand.z-initial.z));
        *reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos)=target;
        Check(MonitorFrame(switchMemory,frame,inverse),"yaw and world position applied to screen");
        const auto originalController=VR().m_rawControllerPose[hand];
        auto setHand=[&](motiongun::Vec local) {
            const auto delta=Sub(motiongun::Transform(frame,local),initialHand);VR().m_rawControllerPose[hand]=originalController;
            VR().m_rawControllerPose[hand].m[0][3]+=delta.x/LiveWorldUnitsPerMetre();
            VR().m_rawControllerPose[hand].m[1][3]-=delta.y/LiveWorldUnitsPerMetre();
            VR().m_rawControllerPose[hand].m[2][3]-=delta.z/LiveWorldUnitsPerMetre();
        };
        uint64_t now=GetTickCount64();
        auto tick=[&](motiongun::Vec local) {setHand(local);AdvancePhysicalMonitor(itemMemory,nullptr,++now);};
        tick({0,-500,-390});Check(!monitorActivations,"tracking first acquired on glass does not accidentally activate");
        tick({0,-500,-300});switchSightBlocked=true;tick({0,-500,-390});
        Check(!monitorActivations,"blocked screen cannot be touched through a wall");switchSightBlocked=false;
        tick({0,-500,-300});*reinterpret_cast<int16_t*>(laraMemory+2)=4;tick({0,-500,-390});
        Check(!monitorActivations,"armed controller does not touch screen");*reinterpret_cast<int16_t*>(laraMemory+2)=0;
        tick({0,-500,-300});g_containerClaimed=true;tick({0,-500,-390});g_containerClaimed=false;
        Check(!monitorActivations,"holding another interaction suppresses screen activation");
        tick({0,-500,-300});g_active=false;tick({0,-500,-390});g_active=true;
        Check(!monitorActivations,"third-person view does not activate touch");
        tick({0,-500,-300});VR().m_controllerPoseValid[hand]=false;AdvancePhysicalMonitor(itemMemory,nullptr,++now);
        VR().m_controllerPoseValid[hand]=true;tick({0,-500,-390});Check(!monitorActivations,"tracking reacquisition is not a touch");
        tick({0,-500,-300});now+=300;tick({0,-500,-390});Check(!monitorActivations,"long tracking gap is not a touch");
        tick({0,-500,-300});monitorRefuse=true;tick({0,-500,-390});
        Check(!monitorTriggers,"failed native activation cannot fire level trigger");monitorRefuse=false;
        tick({0,-500,-300});monitorTriggerChangesLara=true;tick({0,-500,-390});
        Check(monitorActivations==1 && monitorTriggers==1,"either controller touch activates native monitor exactly once");
        Check(*reinterpret_cast<int16_t*>(itemMemory+18)==99 && *reinterpret_cast<int16_t*>(itemMemory+24)==480,"native level-trigger effects survive synthetic animation restoration");
        *reinterpret_cast<int16_t*>(itemMemory+18)=2;*reinterpret_cast<int16_t*>(itemMemory+24)=103;
        for(int i=0;i<5;++i)tick({0,-500,-390});
        tick({0,-500,-300});tick({0,-500,-390});
        Check(monitorActivations==1 && monitorTriggers==1,"native one-shot status prevents resting or repeated touches retriggering");
        ResetPhysicalMonitors();g_monitorDll=nullptr;g_switchDll=nullptr;g_motionDll=nullptr;
    }
}
}
