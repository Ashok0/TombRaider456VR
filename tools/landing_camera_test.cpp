#include <array>
#define main UnusedFullFirstPersonSuite
#include "first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
#include "native_landing_poses.inl"
const NativeLandingPose* landingFixture=nullptr;
void __cdecl LandingJoint(uint8_t* item,tr::PHD_VECTOR* v,int joint,int frac) {
    using namespace tr;
    const auto& p=*reinterpret_cast<PHD_3DPOS*>(item+off::item_pos);
    const auto& old=*reinterpret_cast<PHD_3DPOS*>(item+off::item_pos_prev);
    const float t=frac/256.f,yaw=Radians(old.y_rot)+Wrap(Radians(p.y_rot)-Radians(old.y_rot))*t;
    const auto point=motiongun::Transform(landingFixture->joints[joint],{float(v->x),float(v->y),float(v->z)});
    const auto flat=locomotion::Rotate({point.x,point.z},yaw);
    *v={int32_t(std::lround(old.x_pos+(p.x_pos-old.x_pos)*t+flat.x)),
        int32_t(std::lround(old.y_pos+(p.y_pos-old.y_pos)*t+point.y)),
        int32_t(std::lround(old.z_pos+(p.z_pos-old.z_pos)*t+flat.z))};
}
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

    for(const auto& fixture:nativeLandingPoses) {
        if(fixture.game<4 || fixture.clip==103) continue;
        for(int surface:{0,1,2}) for(bool full:{false,true}) for(bool stable:{false,true}) for(bool calibrated:{false,true})
        for(float size:{.7f,1.f,1.2f}) for(float heading:{0.f,.7f,-1.9f}) for(float duck:{0.f,-.17f,-.4f,.2f}) for(int tracking:{0,1,2,3}) {
            reset(fixture.game-4,full);config.firstPersonMovementStabilization=stable;
            config.firstPersonBodyScale=size;config.firstPersonAnchorX=0;config.firstPersonAnchorY=-32;config.firstPersonAnchorZ=140;
            config.firstPersonPhysicalCrouch=false;
            g_torsoSurface.Reset();
            if(surface) g_torsoSurface.Add(surface==1 ? objectInfo : objectInfo+1,1,
                {-67.2f,-199.6f,-91.7f},{69.2f,-32.9f,64.4f},4185);
            config.positionalTracking=tracking!=1;config.firstPersonHeadTranslation=tracking!=2;
            VR().m_headAtCamera=tracking==3;
            auto& pos=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos);
            auto& prev=*reinterpret_cast<PHD_3DPOS*>(itemMemory+off::item_pos_prev);
            pos=prev={};pos.y_rot=prev.y_rot=Angle(heading);g_headingBase=heading;g_previousBody={};
            *reinterpret_cast<int16_t*>(itemMemory+off::item_anim_number)=int16_t(fixture.clip);
            state=fixture.clip==92?1:2;
            if(calibrated) g_groundEye.Apply({0,0,0},heading,{0,-700,140},heading);
            else g_groundEye.Reset();const auto saved=g_groundEye;
            landingFixture=&fixture;dll.getJointAbsPositionLerp=rva(reinterpret_cast<void*>(&LandingJoint));
            VR().m_rawHeadPose.m[1][3]+=duck;const auto rawHead=VR().m_rawHeadPose;
            for(int f:{0,64,128,192,256}) {
                prev.y_pos=400;pos.y_pos=512;fraction=f;
                const auto physics=std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory));
                PHD_3DPOS camera{};Check(Anchor(camera),"native landing camera is available");
                PHD_VECTOR head{},hip{},raw{0,-32,140};LandingJoint(itemMemory,&head,14,f);LandingJoint(itemMemory,&hip,0,f);LandingJoint(itemMemory,&raw,14,f);
                const float trackingY=tracking==0 ? duck*(config.flipViewY?-1.f:1.f)*LiveWorldUnitsPerMetre() : 0;
                const float anatomicalEyeY=head.y-32.f;
                float expectedY=stable ? std::min(float(raw.y),anatomicalEyeY) : float(raw.y);
                if(stable && full) {
                    PHD_VECTOR left{},right{};LandingJoint(itemMemory,&left,3,f);LandingJoint(itemMemory,&right,6,f);
                    const float footY=float(std::max(left.y,right.y));
                    float supported=footY+(expectedY-footY)*size;
                    if(surface==1) {
                        float top=INFINITY;
                        for(int corner=0;corner<8;++corner) {
                            const motiongun::Vec local{corner&1 ? 69.2f : -67.2f,corner&2 ? -32.9f : -199.6f,corner&4 ? 64.4f : -91.7f};
                            top=std::min(top,motiongun::Transform(fixture.joints[7],local).y);
                        }
                        top+=400+(512-400)*(f/256.f);
                        supported=std::min(supported,footY+(top-footY)*size-.25f*LiveWorldUnitsPerMetre());
                    }
                    expectedY=std::min(expectedY,float(std::lround(supported-trackingY)));
                }
                Check(std::fabs(camera.x_pos-raw.x)<1.1f && std::fabs(camera.y_pos-expectedY)<2.1f && std::fabs(camera.z_pos-raw.z)<1.1f,
                      "landing retains horizontal animation and follows head height without pitching forward clearance into legs");
                if(stable) Check(camera.y_pos<=head.y-31.f,
                      "impact eye anchor remains above the animated neck at every extracted landing key");
                const float renderedY=camera.y_pos+InvertRigid(VR().HeadView()).r[1][3];
                Check(std::fabs(InvertRigid(VR().HeadView()).r[1][3]-trackingY)<.01f,"test uses actual rendered tracking translation");
                Check(std::fabs(renderedY-(expectedY+trackingY))<2.1f,
                      "final eye obeys supported landing depth after applying actual HMD translation");
                Check(physics==std::string(reinterpret_cast<char*>(itemMemory),sizeof(itemMemory)) &&
                    !std::memcmp(&rawHead,&VR().m_rawHeadPose,sizeof(rawHead)),"head following leaves collision and physical tracking untouched");
                Check(!stable || (saved.valid==g_groundEye.valid && saved.local.y==g_groundEye.local.y),"landing cannot recalibrate standing height");
            }
        }
    }
    VR().m_headAtCamera=false;VR().m_system=nullptr;std::printf("PASS: %d landing camera checks\n",checks);
}

