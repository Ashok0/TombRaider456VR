#include <array>
#define main UnusedFullFirstPersonSuite
#include "first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
int main() {
    using namespace tr;
    g_boundBase=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    const auto rva=[](const void* p){return uint32_t(reinterpret_cast<uint64_t>(p)-g_boundBase);};
    dll.lara=rva(laraMemory);dll.laraItem=rva(&laraItem);dll.camera=rva(cameraMemory);
    dll.input=rva(&actionInput);dll.analogInput=rva(analogMemory);dll.frameFrac=rva(&fraction);
    dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&FakeGaitJoint));g_boundDll=&dll;
    alignas(16) static uint8_t objectInfo[off::object_stride]{};
    dll.objects=rva(objectInfo);
    MotionDll motion{};motion.app=rva(&motionApp);g_motionDll=&motion;g_motionHooksReady=true;
    g_hDrawCreatureHD.m_trampoline=reinterpret_cast<void*>(&FakeDraw);
    g_hDrawHair.m_trampoline=reinterpret_cast<void*>(&FakeHair);
    auto& state=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_state);
    auto& status=*reinterpret_cast<int16_t*>(laraMemory+2);
    auto& bits=*reinterpret_cast<uint32_t*>(itemMemory+off::item_mesh_bits);
    const auto reset=[&](int which,bool fullBody) {
        RestoreHeadMesh();ResetMovementStabilization();
        config=Config{};config.enabled=config.firstPerson=config.firstPersonMotionGuns=true;
        config.firstPersonFullBodyIK=fullBody;config.firstPersonUnarmedIK=true;
        config.positionalTracking=config.firstPersonHeadTranslation=true;
        config.firstPersonRoomscaleMove=config.firstPersonWallCollision=config.firstPersonBodyFollowsHead=false;
        game=dll.game=which;
        std::memset(itemMemory,0,sizeof(itemMemory));std::memset(laraMemory,0,sizeof(laraMemory));
        std::memset(appMemory,0,sizeof(appMemory));std::memset(cameraMemory,0,sizeof(cameraMemory));
        *reinterpret_cast<int16_t*>(laraMemory+off::lara_vehicle)=which==0 ? -1 : 0;
        *reinterpret_cast<int16_t*>(itemMemory+off::item_hit_points)=1000;
        state=2;bits=0x7fff;status=0;
        appMemory[0x9e4]=1;water=0;cutseq=cutseqNumber=cutseqTransition=spotCamera=0;
        g_active=g_runtimeInitialized=g_runtimeEnabled=g_haveHeading=g_scenePoseValid=true;
        g_headingItem=itemMemory;g_headingLevel=0;g_headingBase=0;g_scenePose={0,-700,0,0,0,0,0};
        VR().m_system=reinterpret_cast<vr::IVRSystem*>(uintptr_t(1));VR().m_poseValid=true;Head(0);
        VR().RecenterFirstPersonHead();VR().m_controllerPoseValid[0]=VR().m_controllerPoseValid[1]=true;
        for(int hand=0;hand<2;++hand) {
            VR().m_rawControllerPose[hand]=VR().m_rawHeadPose;
            VR().m_rawControllerPose[hand].m[0][3]+=(hand ? .2f : -.2f);
            VR().m_rawControllerPose[hand].m[1][3]-=.3f;
        }
        actionInput=0;std::memset(analogMemory,0,sizeof(analogMemory));fraction=256;
    };

    for(int which:{0,1}) for(bool full:{false,true}) for(bool stable:{false,true}) for(bool calibrated:{false,true})
    for(int clip:{24,31,82,92,99}) for(float heading:{0.f,.7f,-1.9f}) for(float duck:{0.f,-.2f}) {
        reset(which,full);config.firstPersonMovementStabilization=stable;
        config.firstPersonAnchorX=config.firstPersonAnchorY=config.firstPersonAnchorZ=0;
        config.firstPersonPhysicalCrouch=false;
        auto& pos=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
        auto& prev=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos_prev);
        auto& animation=*reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number);
        pos=prev={};pos.y_rot=prev.y_rot=Angle(heading);g_headingBase=heading;
        g_previousBody={};gaitSway={};animation=103;fraction=256;collisionMode=0;
        PHD_3DPOS camera{};Check(Anchor(camera),"standing camera available before fall");
        if(!calibrated) g_groundEye.Reset();const auto saved=g_groundEye;
        VR().m_rawHeadPose.m[1][3]+=duck;const auto head=VR().m_rawHeadPose;
        const float physical=VR().FirstPersonVerticalOffset()*LiveWorldUnitsPerMetre();
        state=clip==92?1:2;animation=int16_t(clip);
        for(int height:{-700,-600,-400,-200,-80,50,-80,-200,-400,-600,-700})
        for(int f:{0,64,128,192,256}) {
            prev.y_pos=400;pos.y_pos=512;fraction=f;gaitSway={30,height+700,-20};
            const auto physics=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
            Check(Anchor(camera),"landing camera available through impact and recovery");
            const float root=400+112*(f/256.f);
            const bool held=stable && calibrated;
            Check(std::fabs(camera.y_pos-(root+(held?saved.local.y:height)))<1.1f,
                  "calibrated landing ignores neck compression while following interpolated collision height");
            if(held) {
                const auto flat=locomotion::Rotate({saved.local.x,saved.local.z},g_headingBase);
                Check(std::fabs(camera.x_pos-flat.x)<1.1f && std::fabs(camera.z_pos-flat.z)<1.1f,
                      "landing camera preserves the calibrated horizontal flight anchor");
                Check(std::fabs((camera.y_pos+physical)-(root+saved.local.y+duck*(config.flipViewY?-1.f:1.f)*LiveWorldUnitsPerMetre()))<1.1f,
                      "physical head lowering remains live and is applied once");
            }
            Check(physics==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)) &&
                  !std::memcmp(&head,&VR().m_rawHeadPose,sizeof(head)),"landing camera cannot alter physics or raw HMD tracking");
            Check(saved.valid==g_groundEye.valid && saved.local.x==g_groundEye.local.x &&
                saved.local.y==g_groundEye.local.y && saved.local.z==g_groundEye.local.z,
                "landing cannot recalibrate standing height");
        }
        state=2;animation=103;gaitSway={};pos.y_pos=prev.y_pos=512;fraction=256;
        Check(Anchor(camera) && camera.y_pos==512-700,"standing recovery has no extra camera rise after stabilized landing");
    }
    VR().m_system=nullptr;std::printf("PASS: %d landing camera checks\n",checks);
}
