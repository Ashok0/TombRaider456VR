// Focused live-elevator regression. Never runs the full first-person suite.
#define main UnusedFullFirstPersonSuite
#include "first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
int main() {
    using namespace tr;
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    const auto rva=[](const void* p){const auto n=reinterpret_cast<uint64_t>(p)-g_boundBase;Check(n<=UINT32_MAX,"fixture RVA fits");return uint32_t(n);};
    dll.lara=rva(laraMemory);dll.laraItem=rva(&laraItem);dll.camera=rva(cameraMemory);
    dll.input=rva(&actionInput);dll.analogInput=rva(analogMemory);g_boundDll=&dll;
    MotionDll motion{};motion.app=rva(&motionApp);g_motionDll=&motion;g_motionHooksReady=true;
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+18);auto& goal=*reinterpret_cast<int16_t*>(itemMemory+20);
    auto& required=*reinterpret_cast<int16_t*>(itemMemory+22);auto& status=*reinterpret_cast<int16_t*>(laraMemory+2);
    const auto reset=[&](int which,int stance,int queued,int busy) {
        config=Config{};config.enabled=config.firstPerson=config.firstPersonMotionGuns=true;
        config.firstPersonFullBodyIK=config.firstPersonUnarmedIK=config.firstPersonPhysicalCrouch=true;
        config.positionalTracking=config.firstPersonHeadTranslation=config.firstPersonMoveWithHead=true;
        config.firstPersonRoomscaleMove=false;game=dll.game=which;
        std::memset(itemMemory,0,sizeof(itemMemory));std::memset(laraMemory,0,sizeof(laraMemory));
        std::memset(appMemory,0,sizeof(appMemory));std::memset(cameraMemory,0,sizeof(cameraMemory));
        *reinterpret_cast<int16_t*>(laraMemory+off::lara_vehicle)=which==0 ? -1 : 0;
        *reinterpret_cast<int16_t*>(itemMemory+off::item_hit_points)=1000;
        state=goal=int16_t(stance);required=int16_t(queued);status=int16_t(busy);
        *reinterpret_cast<int16_t*>(itemMemory+24)=263;
        *reinterpret_cast<uint32_t*>(laraMemory+68)=0x806; // Captured in the TR5 elevator.
        appMemory[0x9e4]=1;water=0;cutseq=cutseqNumber=cutseqTransition=spotCamera=0;
        g_active=g_runtimeEnabled=g_haveHeading=g_scenePoseValid=true;g_headingItem=itemMemory;
        g_headingBase=.4f;g_scenePose={0,-400,0,0,0,0,0};g_haveManualInput=false;
        g_groundJump.Reset();g_heightCrouch={};g_physicalCrouchCamera=false;
        VR().m_system=reinterpret_cast<vr::IVRSystem*>(uintptr_t(1));VR().m_poseValid=true;Head(.7f);
        VR().RecenterFirstPersonHead();VR().m_controllerPoseValid[0]=VR().m_controllerPoseValid[1]=true;
        VR().m_rawControllerPose[0]=VR().m_rawControllerPose[1]=VR().m_rawHeadPose;
        for(int hand=0;hand<2;++hand) VR().m_rawControllerPose[hand].m[1][3]-=.3f;
        actionInput=0;std::memset(analogMemory,0,sizeof(analogMemory));
    };
    // Exact observed condition: ALL4S, queued DUCK, native hands busy.
    reset(1,80,71,1);
    Check(CanSteerCrouch(itemMemory),"elevator crawl keeps steering with queued duck");
    Check(FullBodyIKReady() && UnarmedIKReady(),"native crawl hands-busy keeps tracked hands");
    for(int which:{0,1}) for(int stance:{71,80,81,84,85,86,105,106}) for(int queued:{0,2,71,80,81,84,85,86,105,106}) for(int busy:{0,1}) {
        reset(which,stance,queued,busy);
        Check(CanSteerCrouch(itemMemory) && FullBodyIKReady() && UnarmedIKReady(),"ordinary crouch transitions retain steering and IK");
        const auto saved=std::string(reinterpret_cast<char*>(laraMemory),sizeof(laraMemory));
        motiongun::Frame wrist{};GunBasis controller{};Check(BuildControllerWrist(1,wrist,controller),"crawling controller wrist available");
        const auto before=wrist.origin;VR().m_rawControllerPose[1].m[0][3]+=.1f;
        Check(BuildControllerWrist(1,wrist,controller) && firstperson::Length(Sub(wrist.origin,before))>90,"hand target follows physical motion while busy");
        float x=0,z=1,turn=0;FirstPersonInput(x,z,turn,false);
        Check(g_haveManualInput,"queued crawl posture accepts first-person stick intent");
        const float heading=g_headingBase+VR().HeadYawRadians();
        Check(std::fabs(g_manualWorld.x-std::sin(heading))<.001f && std::fabs(g_manualWorld.z-std::cos(heading))<.001f,"forward intent follows headset yaw");
        g_crouchDrive=true;g_crouchTravelYaw=heading;ApplyCrouchTravelHeading(itemMemory);
        const auto actual=reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos)->y_rot;
        Check(std::abs(int16_t(actual-Angle(heading+(stance==86 ? kPi : 0))))<2,"crawl collision facing follows forward/backward travel");
        Check(status==busy && required==queued,"VR rendering/steering preserves native busy and required state");
        Head(.7f);VR().m_rawHeadPose.m[1][3]-=.65f;
        Check(FirstPersonPhysicalCrouch(true),"low headset maintains physical crouch during queued posture");
        Head(.7f);Check(!FirstPersonPhysicalCrouch(true),"standing headset releases physical crouch request");
        Check(required==queued && status==busy,"physical stand request leaves native ceiling and transition checks authoritative");
    }
    for(int queued:{10,19,40,41,72,88}) {
        reset(1,80,queued,1);Check(!CanSteerCrouch(itemMemory) && !UnarmedIKReady() && !FullBodyIKReady(),"queued interaction/roll/ledge retains native control");
        reset(1,80,0,1);goal=int16_t(queued);Check(!CanSteerCrouch(itemMemory) && !UnarmedIKReady(),"non-posture goal keeps authored hands");
    }
    for(int busy:{1,2,3}) {
        reset(1,2,0,busy);Check(!UnarmedIKReady(),"standing busy/equip states do not gain free controller hands");
    }
    reset(1,80,71,1);*reinterpret_cast<uint32_t*>(laraMemory+68)|=0x20;
    Check(!CanSteerCrouch(itemMemory) && !UnarmedIKReady(),"native auto-alignment still owns movement and hands");
    reset(1,80,71,1);*reinterpret_cast<uint32_t*>(itemMemory+0x1820)|=8;
    Check(!CanSteerCrouch(itemMemory) && !UnarmedIKReady(),"airborne crawl does not bypass guards");
    reset(1,80,71,1);g_active=false;
    Check(!FullBodyIKReady() && !UnarmedIKReady() && !FirstPersonPhysicalCrouch(true),"third-person posture remains native");
    std::printf("PASS: %d focused crouch-transition checks (TR4/5)\n",checks);
}
