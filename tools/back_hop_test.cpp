// Focused RG/back-hop regression. Uses stubbed engine callbacks only;
// the full self-test and game DLL functions are never executed.
#define main UnusedFullFirstPersonSuite
#include "first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
namespace {
bool captureOnly=false;
void __cdecl BackHopAboveWater(uint8_t* item,void* collision) {
    seenInput=actionInput;
    std::memcpy(seenAnalog,analogMemory,sizeof(seenAnalog));
    if(captureOnly) return;
    auto& state=*reinterpret_cast<int16_t*>(item+tr::off::item_anim_state);
    auto& goal=*reinterpret_cast<int16_t*>(item+tr::off::item_goal_state);
    auto& anim=*reinterpret_cast<int16_t*>(item+tr::off::item_anim_number);
    // Native STOP selects FASTBACK for Back without Walk, after rear-floor
    // clearance. FASTBACK requests STOP but completes its own displacement.
    if(state==2 && (actionInput&tr::locomotion::Back) && !(actionInput&tr::locomotion::Walk)) {
        if(blockGaitStart) return;
        const auto yaw=reinterpret_cast<const tr::PHD_3DPOS*>(item+tr::off::item_pos)->y_rot;
        Check(!analogMemory[0] && !analogMemory[1] && analogMemory[2]==yaw,
            "hop bypasses analog turning and uses Lara facing, armed or unarmed");
        state=5;goal=2;anim=88;
    }
    if(state!=5) { FakeImmediateGaitAboveWater(item,collision);return; }
    goal=2;animationNextState=-1;
    FakeUnstableAboveWater(item,collision);
}
}
int main() {
    using namespace tr;
    using namespace tr::locomotion;
    Check(!Config{}.hybridModernTankControls,"Hybrid Modern/Tank Controls defaults Off");
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    const auto rva=[](const void* p) { const auto n=reinterpret_cast<uint64_t>(p)-g_boundBase;Check(n<=UINT32_MAX,"fixture RVA fits");return uint32_t(n); };
    dll.lara=rva(laraMemory);dll.laraItem=rva(&laraItem);dll.camera=rva(cameraMemory);
    dll.input=rva(&actionInput);dll.analogInput=rva(analogMemory);g_boundDll=&dll;
    MotionDll motion{};motion.app=rva(&motionApp);g_motionDll=&motion;g_motionHooksReady=true;
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+18);auto& goal=*reinterpret_cast<int16_t*>(itemMemory+20);
    auto& anim=*reinterpret_cast<int16_t*>(itemMemory+24);auto& pos=*reinterpret_cast<PHD_3DPOS*>(itemMemory+96);
    const auto reset=[&](int which,bool modern,bool armed,bool smooth,float head,int clip) {
        config=Config{};config.enabled=config.firstPerson=config.gamepadEnabled=true;
        config.hybridModernTankControls=true;
        config.firstPersonRoomscaleMove=false;config.firstPersonMovementStabilization=smooth;
        game=dll.game=which;water=0;cutseq=cutseqNumber=cutseqTransition=spotCamera=0;
        std::memset(itemMemory,0,sizeof(itemMemory));std::memset(laraMemory,0,sizeof(laraMemory));
        std::memset(appMemory,0,sizeof(appMemory));std::memset(cameraMemory,0,sizeof(cameraMemory));
        std::memset(analogMemory,0,sizeof(analogMemory));actionInput=Back|Walk;seenInput=0;
        *reinterpret_cast<int16_t*>(laraMemory+off::lara_vehicle)=which==0 ? -1 : 0;
        *reinterpret_cast<int16_t*>(itemMemory+off::item_hit_points)=1000;
        *reinterpret_cast<int16_t*>(laraMemory+2)=armed ? 4 : 0;
        *reinterpret_cast<int16_t*>(laraMemory+4)=armed ? 1 : 0;
        *reinterpret_cast<int32_t*>(appMemory+drva::app_off::cfgFlags)=modern ? 2 : 0;
        state=goal=clip==103 ? 2 : clip==40 ? 16 : clip==12 ? 6 : clip==13 ? 7 : 20;anim=int16_t(clip);
        g_active=g_runtimeEnabled=g_haveHeading=g_scenePoseValid=false;g_headingItem=nullptr;
        pos.y_rot=Angle(head);analogMemory[2]=analogMemory[3]=Angle(head+1.2f);
        g_headingBase=head;g_scenePose={0,-700,0,0,0,0,0};g_haveManualInput=false;
        g_groundJump.Reset();g_jumpLanding.Reset();g_heightCrouch={};g_physicalCrouchCamera=false;
        ResetMovementStabilization();
        VR().m_system=reinterpret_cast<vr::IVRSystem*>(uintptr_t(1));VR().m_poseValid=true;Head(0);
        VR().RecenterFirstPersonHead();
        std::memset(immediateGaitAnimations,0,sizeof(immediateGaitAnimations));
        *reinterpret_cast<int16_t*>(immediateGaitAnimations+11*48+10)=2;
        *reinterpret_cast<int16_t*>(immediateGaitAnimations+11*48+28)=184;
        *reinterpret_cast<int16_t*>(immediateGaitAnimations+11*48+30)=185;
        dll.anims=rva(&immediateGaitTable);
        g_hAnimateLara.m_trampoline=reinterpret_cast<void*>(&FakeGaitTransitionAnimate);
        g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&BackHopAboveWater);
        blockGaitStart=blockStableMotion=collisionStartsFall=captureOnly=false;collisionPush={};animationNextState=-1;
    };
    const auto input=[](float x,float z,bool grip,bool shifted=false) {
        float turn=0;FirstPersonInput(x,z,turn,shifted,false,grip);
        analogMemory[0]=int16_t(x*32767);analogMemory[1]=int16_t(z*32767);
    };
    for(int which:{0,1}) for(bool modern:{false,true}) for(bool armed:{false,true})
    for(bool smooth:{false,true}) for(float head:{0.f,.7f}) for(int clip:{103,40,12,13,44,69}) {
        reset(which,modern,armed,smooth,head,clip);
        config.hybridModernTankControls=false;captureOnly=true;
        float x=.1f,z=-1,turn=.3f;
        FirstPersonInput(x,z,turn,false,false,true);
        Check(!g_thirdPersonBackHopPressed && x==.1f && z==-1 && turn==.3f,
            "disabled hybrid preserves third-person stick input without queuing a hop");
        analogMemory[0]=1234;analogMemory[1]=-23456;
        const int originalState=state,originalAnim=anim;
        const int16_t originalAxes[4]={analogMemory[0],analogMemory[1],analogMemory[2],analogMemory[3]};
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(seenInput==(Back|Walk) && !std::memcmp(seenAnalog,originalAxes,sizeof(originalAxes)),
            "disabled hybrid forwards native Walk/direction bits and camera-relative analog axes");
        Check(state==originalState && anim==originalAnim,"disabled hybrid does not force a standing or hop animation");
        g_thirdPersonBackHopPressed=true;
        { ThirdPersonBackHopScope stale(itemMemory);
          Check(actionInput==(Back|Walk) && !std::memcmp(analogMemory,originalAxes,sizeof(originalAxes)),
              "disabled hybrid also rejects a previously queued hop at simulation time"); }
    }
    for(int which:{0,1}) for(bool modern:{false,true}) for(bool armed:{false,true})
    for(bool smooth:{false,true}) for(float head:{0.f,.7f}) for(int clip:{103,40,12,13,44,69}) {
        reset(which,modern,armed,smooth,head,clip);input(0,-1,true);
        const int ticks=animationTicks,collisions=nativeCollisionPasses;
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(state==5 && (seenInput&(Directions|Walk))==Back,"RG/back enters native hop from idle or backward walk");
        Check(std::fabs(Length(beforeNativeCollision)-12)<1,"hop displacement is neither canceled nor tripled");
        Check(Dot(beforeNativeCollision,Rotate({0,-1},head))>11,"third-person hop moves backward relative to Lara");
        Check(animationTicks==ticks+1 && nativeCollisionPasses==collisions+1 && pos.y_pos==7,"native animation, height and collision run once");
        Check(analogMemory[1]!=0 && analogMemory[2]==Angle(head+1.2f) && actionInput==(Back|Walk),"native input and chase-camera axes restored after simulation");
        for(const Vec stick:{Vec{},Vec{0,1},Vec{1,0},Vec{0,-1}}) {
            actionInput=0;input(stick.x,stick.z,false);Detour_LaraAboveWater(itemMemory,nullptr);
            Check(state==5 && std::fabs(Length(beforeNativeCollision)-12)<1,"release/reversal does not cancel or scale an unfinished hop");
            Check(Dot(beforeNativeCollision,Rotate({0,-1},head))>11,"unfinished hop retains its travel direction");
        }
        blockStableMotion=true;const auto before=pos;Detour_LaraAboveWater(itemMemory,nullptr);
        Check(pos.x_pos==before.x_pos && pos.z_pos==before.z_pos,"native wall collision remains final");
    }
    for(int which:{0,1}) {
        for(bool hybrid:{false,true}) for(bool grip:{false,true}) {
            reset(which,true,false,true,0,103);
            config.hybridModernTankControls=hybrid;
            g_active=g_runtimeEnabled=g_haveHeading=true;g_headingItem=itemMemory;
            input(0,-1,grip);Detour_LaraAboveWater(itemMemory,nullptr);
            Check(state==16 && (seenInput&Walk),"first-person movement is unchanged with RG held or released");
        }
        reset(which,true,false,true,0,103);input(0,-1,true);
        *reinterpret_cast<int16_t*>(itemMemory+22)=71;Detour_LaraAboveWater(itemMemory,nullptr);
        Check(state==5 && *reinterpret_cast<int16_t*>(itemMemory+22)==71,"standing queued DUCK in VCI permits hop without clearing native queue");
        for(const Vec stick:{Vec{},Vec{0,1},Vec{1,0},Vec{-1,0}}) {
            reset(which,true,false,true,0,103);captureOnly=true;input(stick.x,stick.z,true);
            Detour_LaraAboveWater(itemMemory,nullptr);
            Check(seenInput==(Back|Walk),"neutral, forward and sideways sticks do not activate hop");
        }
        reset(which,true,false,true,0,103);input(0,-1,true);blockGaitStart=true;Detour_LaraAboveWater(itemMemory,nullptr);
        Check(state==2 && pos.z_pos==0,"native rear-floor rejection prevents the hop");
        for(int guard=0;guard<16;++guard) {
            reset(which,true,false,true,0,103);captureOnly=true;
            if(guard==0) g_active=true;
            if(guard==1) state=goal=86;
            if(guard==2) state=goal=80;
            if(guard==3) g_runtimeEnabled=true;
            if(guard==4) actionInput|=firstperson::PhysicalCrouchHoldScope::Duck;
            if(guard==5) actionInput|=0x40;
            if(guard==6) actionInput|=0x4000;
            if(guard==7) actionInput|=0x100;
            if(guard==8) actionInput|=0x10;
            if(guard==9) *reinterpret_cast<int16_t*>(itemMemory+22)=56;
            if(guard==10) *reinterpret_cast<uint32_t*>(itemMemory+0x1820)|=8;
            if(guard==11) anim=24;
            if(guard==13) config.gamepadEnabled=false;
            if(guard==14) *reinterpret_cast<uint32_t*>(laraMemory+68)|=0x20;
            input(0,-1,guard!=15,guard==12);Detour_LaraAboveWater(itemMemory,nullptr);
            Check((seenInput&Walk)!=0,"first person (including suspended), vents, jump, Action, roll and shifted input are untouched");
        }
    }
    std::printf("PASS: %d third-person hop and first-person preservation checks (TR4/5)\n",checks);
}
