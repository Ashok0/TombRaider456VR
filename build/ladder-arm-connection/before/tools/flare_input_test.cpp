// Only flare readiness/controller integration executes, never the full suite.
#define main UnusedFullFirstPersonSuite
#include "first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
int main() {
    using namespace tr;
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    const auto rva=[](const void* p){return uint32_t(reinterpret_cast<uint64_t>(p)-g_boundBase);};
    dll.lara=rva(laraMemory);dll.laraItem=rva(&laraItem);dll.camera=rva(cameraMemory);dll.input=rva(&actionInput);
    g_boundDll=&dll;MotionDll motion{};motion.app=rva(&motionApp);g_motionDll=&motion;
    FlareThrowDll flare{};flare.items=rva(&loadedRooms);g_flareThrowDll=&flare;
    auto setup=[&](int which){
        config=Config{};config.firstPerson=config.enabled=true;
        config.positionalTracking=config.firstPersonHeadTranslation=config.firstPersonMotionGuns=true;
        game=dll.game=which;g_active=g_runtimeEnabled=g_haveHeading=g_scenePoseValid=true;
        g_motionHooksReady=g_flareHooksReady=true;g_headingItem=itemMemory;g_headingBase=0;
        std::memset(itemMemory,0,sizeof(itemMemory));std::memset(laraMemory,0,sizeof(laraMemory));
        std::memset(cameraMemory,0,sizeof(cameraMemory));std::memset(appMemory,0,sizeof(appMemory));
        *reinterpret_cast<int16_t*>(itemMemory+18)=2;*reinterpret_cast<int16_t*>(itemMemory+38)=1000;
        *reinterpret_cast<int16_t*>(laraMemory+off::lara_vehicle)=which==0?-1:0;
        *reinterpret_cast<int16_t*>(laraMemory+4)=7;appMemory[0x9e4]=1;water=0;
        VR().m_system=reinterpret_cast<vr::IVRSystem*>(1);VR().m_poseValid=true;
        Head(0);VR().RecenterFirstPersonHead();
        VR().m_controllerPoseValid[0]=VR().m_controllerPoseValid[1]=true;
        for(auto& pose:VR().m_rawControllerPose)pose=VR().m_rawHeadPose;
        ResetPhysicalHolsters();g_flareGesture={};g_flareRelease={};
    };
    for(int which:{0,1}) {
        setup(which);Check(FlareIKReady(),"real tracked-flare eligibility accepts TR4/5");
        VRSystem::HandState hands[2]{};hands[0].valid=hands[1].valid=true;hands[0].grip=1;
        XState out{};bool shifted=false,action=false;
        BuildStateFromHands(hands,out,shifted,true,action);
        Check(g_flareGesture.count>0 && (out.Gamepad.wButtons&XB_LEFT_SHOULDER),"left grip still crouches with a held flare");
        g_flareGesture.armed=true;
        hands[1].grip=1;BuildStateFromHands(hands,out,shifted,true,action);
        Check(action && !g_flareGesture.armed && !g_flareRelease.time,"two grips cancel throw and retain Action");
        hands[0].grip=hands[1].grip=0;BuildStateFromHands(hands,out,shifted,true,action);
        Check(!g_flareRelease.time,"releasing Action chord cannot throw");
        Check(g_flareGesture.count>0 && !(out.Gamepad.wButtons&XB_LEFT_SHOULDER),"swing detector samples with no grip pressed");
        for(int exclusion=0;exclusion<8;++exclusion){
            setup(which);
            switch(exclusion){case 0:g_active=false;break;case 1:appMemory[0x9e4]=0;break;
            case 2:VR().m_controllerPoseValid[0]=false;break;case 3:*reinterpret_cast<int16_t*>(laraMemory+2)=3;break;
            case 4:*reinterpret_cast<int16_t*>(laraMemory+4)=0;break;case 5:water=1;break;
            case 6:*reinterpret_cast<int16_t*>(itemMemory+18)=56;break;case 7:config.firstPersonMotionGuns=false;break;}
            FirstPersonFlareMotion(true);Check(g_flareGesture.count==0,"native controls retained outside physical flare eligibility");
        }
        for(int chord=0;chord<5;++chord){
            setup(which);hands[0]={};hands[1]={};hands[0].valid=hands[1].valid=true;hands[0].grip=1;
            BuildStateFromHands(hands,out,shifted,true,action);
            g_flareGesture.armed=true;
            switch(chord){case 0:hands[0].btnUpper=true;break;case 1:hands[0].btnLower=true;break;
            case 2:hands[0].trigger=1;break;case 3:hands[1].trigger=1;break;
            case 4:hands[0].stickClick=hands[1].stickClick=true;break;}
            BuildStateFromHands(hands,out,shifted,true,action);
            Check(!g_flareGesture.armed && !g_flareRelease.time,"menu/equip/photo chords cancel prepared throw");
        }
    }
    std::printf("PASS: %d flare controller integration checks\n",checks);
}
