// Focused VCI movement regression; never executes the full first-person suite.
#define main UnusedFullFirstPersonSuite
#include "first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
int main() {
    using namespace tr;
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    const auto rva=[](const void* p) { const auto n=reinterpret_cast<uint64_t>(p)-g_boundBase;Check(n<=UINT32_MAX,"fixture RVA fits");return uint32_t(n); };
    dll.lara=rva(laraMemory);dll.laraItem=rva(&laraItem);dll.camera=rva(cameraMemory);
    dll.input=rva(&actionInput);dll.analogInput=rva(analogMemory);g_boundDll=&dll;
    MotionDll motion{};motion.app=rva(&motionApp);g_motionDll=&motion;g_motionHooksReady=true;
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+18);auto& goal=*reinterpret_cast<int16_t*>(itemMemory+20);
    auto& required=*reinterpret_cast<int16_t*>(itemMemory+22);auto& anim=*reinterpret_cast<int16_t*>(itemMemory+24);
    auto& frame=*reinterpret_cast<int16_t*>(itemMemory+26);auto& pos=*reinterpret_cast<PHD_3DPOS*>(itemMemory+96);
    const auto reset=[&](int which,int queued,int oldAnim,float heading) {
        config=Config{};config.enabled=config.firstPerson=config.gamepadEnabled=true;
        config.firstPersonRoomscaleMove=false;
        game=dll.game=which;water=0;cutseq=cutseqNumber=cutseqTransition=spotCamera=0;
        std::memset(itemMemory,0,sizeof(itemMemory));std::memset(laraMemory,0,sizeof(laraMemory));
        std::memset(appMemory,0,sizeof(appMemory));std::memset(cameraMemory,0,sizeof(cameraMemory));
        std::memset(analogMemory,0,sizeof(analogMemory));actionInput=0;
        *reinterpret_cast<int16_t*>(laraMemory+off::lara_vehicle)=which==0 ? -1 : 0;
        *reinterpret_cast<int16_t*>(itemMemory+off::item_hit_points)=1000;
        *reinterpret_cast<uint32_t*>(laraMemory+68)=0x816; // Live VCI standing flags.
        state=goal=oldAnim==103 ? 2 : oldAnim==65 || oldAnim==66 ? 22 : oldAnim==67 || oldAnim==68 ? 21 : oldAnim==40 || oldAnim==41 ? 16 : 1;
        anim=int16_t(oldAnim);frame=oldAnim==103 ? 2202 : 5;required=int16_t(queued);
        g_active=g_runtimeEnabled=g_haveHeading=g_scenePoseValid=true;g_headingItem=itemMemory;
        g_headingBase=heading;g_scenePose={0,-700,0,0,0,0,0};g_haveManualInput=false;
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
        g_hLaraAboveWater.m_trampoline=reinterpret_cast<void*>(&FakeImmediateGaitAboveWater);
        blockGaitStart=blockStableMotion=collisionStartsFall=false;collisionPush={};animationNextState=-1;
    };
    struct Direction {float x,z,offset;int gait;uint64_t action;};
    const Direction directions[]={{0,1,0,1,locomotion::Forward},{0,-1,kPi,16,locomotion::Back},
        {-1,0,-kPi/2,22,locomotion::StepLeft},{1,0,kPi/2,21,locomotion::StepRight}};
    // Reproduce the live STOP/idle 103/required DUCK condition first.
    reset(1,71,103,0);float x=0,z=1,r=0;FirstPersonInput(x,z,r,false);
    Detour_LaraAboveWater(itemMemory,nullptr);
    Check(state==1 && locomotion::Length(beforeNativeCollision)>0,"VCI queued duck does not delay standing stick movement");
    for(int which:{0,1}) for(int queued:{0,71}) for(bool stable:{false,true})
    for(int modern:{0,1}) for(float heading:{0.f,.73f}) for(const auto& d:directions)
    for(int oldAnim:{103,0,40,65,66,67,68}) {
        reset(which,queued,oldAnim,heading);config.firstPersonMovementStabilization=stable;
        *reinterpret_cast<int32_t*>(appMemory+drva::app_off::cfgFlags)=modern ? 2 : 0;
        // Ensure this is a handoff, including a re-press during pending stop.
        if(state==d.gait) goal=2;
        x=d.x;z=d.z;r=0;FirstPersonInput(x,z,r,false);
        const int animations=animationTicks,collisions=nativeCollisionPasses;
        const auto before=pos;Detour_LaraAboveWater(itemMemory,nullptr);
        Check(state==d.gait && locomotion::Length(beforeNativeCollision)>0,"every direction starts on the first simulation tick");
        Check(required==queued,"native queued posture is preserved");
        Check(animationTicks==animations+1 && nativeCollisionPasses==collisions+1 && pos.y_pos==before.y_pos+7,"one native animation/collision tick retains vertical motion");
        Check(std::fabs(beforeNativeCollision.x*std::cos(heading+d.offset)-beforeNativeCollision.z*std::sin(heading+d.offset))<2,"movement follows headset-relative direction");
        const int starts=standingGaitChecks;Detour_LaraAboveWater(itemMemory,nullptr);
        Check(standingGaitChecks==starts,"held gait does not restart every tick");
    }
    for(int which:{0,1}) for(int queued:{0,71}) {
        reset(which,queued,103,0);blockGaitStart=true;x=0;z=1;r=0;FirstPersonInput(x,z,r,false);
        Detour_LaraAboveWater(itemMemory,nullptr);
        Check(state==2 && locomotion::Length(beforeNativeCollision)==0,"native floor/ceiling rejection still prevents entry");
        reset(which,queued,103,0);blockStableMotion=true;x=1;z=0;FirstPersonInput(x,z,r,false);
        const auto before=pos;Detour_LaraAboveWater(itemMemory,nullptr);
        Check(locomotion::Length(beforeNativeCollision)>0 && pos.x_pos==before.x_pos && pos.z_pos==before.z_pos,"native wall collision retains final position");
        reset(which,queued,41,0);state=goal=16;frame=744;
        auto* start=immediateGaitAnimations+41*48;
        *reinterpret_cast<int16_t*>(start+10)=16;*reinterpret_cast<int16_t*>(start+28)=744;*reinterpret_cast<int16_t*>(start+30)=759;
        AccelerateBackpedalStart(itemMemory,locomotion::Back);
        Check(frame==747 && required==queued,"queued duck retains the existing fast backward startup without clearing queue");
    }
    for(int guard=0;guard<10;++guard) {
        reset(1,71,103,0);
        if(guard==0) required=56;
        if(guard==1) goal=71;
        if(guard==2) actionInput=firstperson::PhysicalCrouchHoldScope::Duck;
        if(guard==3) g_heightCrouch.active=true;
        if(guard==4) g_active=false;
        if(guard==5) *reinterpret_cast<uint32_t*>(laraMemory+68)|=0x20;
        if(guard==6) *reinterpret_cast<uint32_t*>(itemMemory+0x1820)|=8;
        if(guard==7) anim=24;
        if(guard==8) actionInput=0x40;
        if(guard==9) state=goal=80;
        const auto saved=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
        PrepareGroundDirection(itemMemory,locomotion::StepLeft);
        Check(!std::memcmp(saved.data(),itemMemory,sizeof(itemMemory)),"crouch/interaction/third-person/airborne/authored transitions remain native");
    }
    std::printf("PASS: %d focused ground-queue checks (TR4/5)\n",checks);
}
